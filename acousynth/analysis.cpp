#include "analysis.hpp"
#include "macros.hpp"
#include "input_config.hpp"
#include "libs/kissfft/kiss_fftr.h"
#include <stdio.h> //  for printf


// --- Analysis Parameters ---
// Define peak-finding sensitivity.
#define PEAK_THRESHOLD 0.05f
// Define envelope attack/decay sensitivity.
#define ENV_THRESHOLD 0.05f
// How many frames a peak must be stable to be "played".
#define STABILITY_COUNT 3
// How many bins to check on either side for peak detection.
static int MODES_RESOLUTION;

// --- Buffers & FFT Config ---
// Internal buffer for 50% overlap processing
static float processing_buffer[I_BUFFER_SIZE];
// Pre-calculated Hanning window
static float hanning_window[I_BUFFER_SIZE];

// KissFFT configuration and buffers
static kiss_fftr_cfg fft_cfg;           // real FFT config
static float fft_in_r[I_BUFFER_SIZE];     // input buffer
static kiss_fft_cpx fft_out_cpx[FFT_SIZE / 2 + 1]; // output is N/2 + 1 complex points



/**
 * @brief (Internal) Checks if a frequency bin is a local peak.
 */
static bool is_peak(float* amps, int k, int num_freqs) {
    if (k - MODES_RESOLUTION <= 0 || k + MODES_RESOLUTION >= num_freqs - 1) {
        return false;
    }
    if (amps[k] < PEAK_THRESHOLD) {
        return false;
    }
    for (int i = 1; i <= MODES_RESOLUTION; i++) {
        if (amps[k - i] >= amps[k] || amps[k + i] >= amps[k]) {
            return false;
        }
    }
    return true;
}

/**
 * @brief
(Internal) Determines envelope phase (attack, sustain, decay).
 */
static int get_env_phase(float amp_now, float amp_prev) {
    float diff = amp_now - amp_prev;
    if (diff > ENV_THRESHOLD) return 1;  // Attack
    if (diff < -ENV_THRESHOLD) return -1; // Decay
    return 0; // Sustain
}

/**
 * @brief Initializes analysis components.
 */
void analysis_init() {
    // 1. Calculate Hanning window
    for (int i = 0; i < I_BUFFER_SIZE; i++) {
        hanning_window[i] = 0.5f - 0.5f * cosf(2.0f * M_PI * i / (I_BUFFER_SIZE - 1));
    }

    // 2. Clear processing buffer
    memset(processing_buffer, 0, sizeof(processing_buffer));

    // 3. Initialize KissFFT
    fft_cfg = kiss_fftr_alloc(I_BUFFER_SIZE, 0, NULL, NULL);

    // 4. Calculate mode resolution for peak detection
    float resolution = (float)FS_I / (float)I_BUFFER_SIZE;
    MODES_RESOLUTION = (int)(4.9f / resolution); // 4.9 Hz is is the minimal frequency separation for guitar notes
    //if (MODES_RESOLUTION < 1) MODES_RESOLUTION = 1; // CHECK!!
    printf("[Analysis] Init Complete. Res: %.2f Hz/bin\n", resolution);

}

/**
 * @brief Analyzes one audio segment: performs STFT and updates frq_array.
 */
void analyze_audio_segment(int16_t* new_samples) {
    printf("Raw ADC: %d\n", new_samples[0]); 
    //Shift old data (overlap)
    // Move the second half (HOP_SIZE) to the first half
    size_t samples_to_keep = I_BUFFER_SIZE - HOP_SIZE;
    memmove(processing_buffer, 
            &processing_buffer[HOP_SIZE], 
            samples_to_keep * sizeof(float));

    //Read and normalize new data into the second half
    for (int i = 0; i < HOP_SIZE; i++) {
        // Normalize from int16 (-32768 to 32767) to float (-1.0 to 1.0)
        //processing_buffer[HOP_SIZE + i] = ((float)new_samples[i] - 32767.0f) / 32768.0f;
        processing_buffer[samples_to_keep + i] = ((float)new_samples[i] - 2048.0f) / 2048.0f;
    }

    //Apply window and prepare for FFT
    for (int i = 0; i < I_BUFFER_SIZE; i++) {
        fft_in_r[i] = processing_buffer[i] * hanning_window[i];
    }

    //Run FFT
    kiss_fftr(fft_cfg, fft_in_r, fft_out_cpx);

    //Calculate Amplitudes and update frq_array
    
    // Temporary array to hold all current amplitudes for peak detection
    float current_amps[NUM_FREQS]; //fft_out_cpx size is I_BUFFER_SIZE, but we only need first NUM_FREQS (FFT_SIZE/2)

    // First pass: Calculate all amplitudes
    for (int k = 0; k < NUM_FREQS; k++) {
        float real = fft_out_cpx[k].r;
        float imag = fft_out_cpx[k].i;

        // Get normalized amplitude (0.0 to 1.0)
        // (I_BUFFER_SIZE / 2) is the normalization factor for a real signal
        float new_amp = sqrtf(real * real + imag * imag) / (I_BUFFER_SIZE / 2);
        current_amps[k] = new_amp;
    }
    printf("[Analysis] FFT Complete.\n");

    // Second pass: Detect peaks and update the shared `frq_array`
    for (int k = 0; k < NUM_FREQS; k++) {
        FreqData* current_freq = &frq_array[k];
        float new_amp = current_amps[k];
        float prev_amp = current_freq->amp_float; // Get last frame's amp

        current_freq->is_peak = is_peak(current_amps, k, NUM_FREQS);

        if (current_freq->is_peak) {
            current_freq->env_phase = get_env_phase(new_amp, prev_amp);
            current_freq->stability++;
            
            if (current_freq->stability > STABILITY_COUNT) {
                current_freq->play = true;
                // Set the amplitude for the synthesizer (convert 0.0-1.0 to Q15)
                // Boost amp
                float boosted_amp = new_amp * AMP_CORRECTION_FACTOR;
                if (boosted_amp > 1.0f) boosted_amp = 1.0f;
                current_freq->amp = (int16_t)(boosted_amp * 32767.0f);
            }
        } else {
            current_freq->env_phase = 0; // 0 for "None"
            current_freq->stability = 0;
            current_freq->play = false;
            current_freq->amp = 0; // Stop playing this freq
        }

        // Store current amp for next frame's comparison (raw amp)
        current_freq->amp_float = new_amp;
    }
    printf("[Analysis] FFT Analyzed.\n");

    // --- DEBUG DATA COLLECTION ---
    int active_peaks = 0;
    
    for (int k = 0; k < NUM_FREQS; k++) {
        FreqData* current_freq = &frq_array[k];
        float new_amp = current_amps[k];
        float prev_amp = current_freq->amp_float; 

        current_freq->is_peak = is_peak(current_amps, k, NUM_FREQS);

        if (current_freq->is_peak) {
            current_freq->env_phase = get_env_phase(new_amp, prev_amp);
            current_freq->stability++;
            
            if (current_freq->stability > STABILITY_COUNT) {
                current_freq->play = true;
                current_freq->amp = (int16_t)(new_amp * 32767.0f);
                active_peaks++; // Count for debug
            }
        } else {
            current_freq->env_phase = 0;
            current_freq->stability = 0;
            current_freq->play = false;
            current_freq->amp = 0; 
        }
        current_freq->amp_float = new_amp;
    }

    // --- DEBUG PRINTOUT ---
    // Only print if there is something interesting, or periodically
    if (active_peaks > 0) {
        printf("\n[Analysis] Peaks Found: %d\n", active_peaks);
        float bin_res = (float)FS_I / (float)FFT_SIZE;
        
        for (int k = 0; k < NUM_FREQS; k++) {
            if (frq_array[k].play) {
                // Calculate Hz for display
                float freq = k * bin_res;
                printf(" -> Bin %d: %.1f Hz (Amp: %d)\n", k, freq, frq_array[k].amp);
            }
        }
    }
}