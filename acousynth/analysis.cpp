#include "analysis.hpp"
#include "macros.hpp"
#include "input_config.hpp"
#include "libs/kissfft/kiss_fftr.h"
#include <stdio.h> 
#include <string.h> // for memset, memmove

// --- Debug Toggle ---
// Comment this out for production (saves UART time)
//#define DEBUG_ANALYSIS 

// --- Constants ---
constexpr float PEAK_THRESHOLD = 0.01f;   // Minimum amplitude to consider
constexpr float ENV_THRESHOLD  = 0.05f;   // Change detection for Attack/Decay
constexpr int   STABILITY_COUNT = 2;      // Frames required to lock a note
constexpr float ADC_BIAS = 2048.0f;       // 12-bit ADC Center
constexpr float MIN_FREQ_SEP = 4.9f;      // Min Hz separation for guitar notes

// --- Internal State ---
static int MODES_RESOLUTION;
static float processing_buffer[I_BUFFER_SIZE];
static float hanning_window[I_BUFFER_SIZE];

// KissFFT State
static kiss_fftr_cfg fft_cfg;           
static float fft_in_r[I_BUFFER_SIZE];     
static kiss_fft_cpx fft_out_cpx[FFT_SIZE / 2 + 1]; 

// --- Helper Functions ---

static bool is_peak(float* amps, int k, int num_freqs) {
    if (k - MODES_RESOLUTION <= 0 || k + MODES_RESOLUTION >= num_freqs - 1) return false;
    
    // 1. Threshold Check
    if (amps[k] < PEAK_THRESHOLD) return false;

    // 2. Local Maxima Check
    for (int i = 1; i <= MODES_RESOLUTION; i++) {
        if (amps[k - i] > amps[k] || amps[k + i] >= amps[k]) return false;
    }
    return true;
}

static int get_env_phase(float amp_now, float amp_prev) {
    float diff = amp_now - amp_prev;
    if (diff > ENV_THRESHOLD) return 1;  // Attack
    if (diff < -ENV_THRESHOLD) return -1; // Decay
    return 0; // Sustain
}

// --- Public Functions ---

void analysis_init() {
    // 1. Pre-calc Window
    for (int i = 0; i < I_BUFFER_SIZE; i++) {
        hanning_window[i] = 0.5f - 0.5f * cosf(2.0f * M_PI * i / (I_BUFFER_SIZE - 1));
    }

    // 2. Clear Buffers
    memset(processing_buffer, 0, sizeof(processing_buffer));

    // 3. Alloc FFT
    fft_cfg = kiss_fftr_alloc(I_BUFFER_SIZE, 0, NULL, NULL);

    // 4. Calc Resolution
    float bin_width_hz = (float)FS_I / (float)I_BUFFER_SIZE;
    MODES_RESOLUTION = (int)(MIN_FREQ_SEP / bin_width_hz);
    if (MODES_RESOLUTION < 1) MODES_RESOLUTION = 1;

    printf("[Analysis] Init. Res: %.2f Hz/bin, Search Radius: %d bins\n", bin_width_hz, MODES_RESOLUTION);
}

void analyze_audio_segment(int16_t* new_samples) {
    
    // 1. Sliding Window (Overlap)
    // Shift old data left
    size_t samples_to_keep = I_BUFFER_SIZE - HOP_SIZE;
    memmove(processing_buffer, &processing_buffer[HOP_SIZE], samples_to_keep * sizeof(float));

    // 2. Normalize New Data (Int16 -> Float -1.0 to 1.0)
    for (int i = 0; i < HOP_SIZE; i++) {
        processing_buffer[samples_to_keep + i] = ((float)new_samples[i] - ADC_BIAS) / ADC_BIAS;
    }

    // 3. Apply Window & Prepare FFT
    for (int i = 0; i < I_BUFFER_SIZE; i++) {
        fft_in_r[i] = processing_buffer[i] * hanning_window[i];
    }

    // 4. Execute FFT
    kiss_fftr(fft_cfg, fft_in_r, fft_out_cpx);

    // 5. Calculate Magnitudes (First Pass)
    // We need all amplitudes calculated before checking neighbors for peaks
    float current_amps[NUM_FREQS]; 
    for (int k = 0; k < NUM_FREQS; k++) {
        // Normalization: Divide by N/2
        float norm = (I_BUFFER_SIZE / 2.0f);
        float mag = sqrtf(fft_out_cpx[k].r * fft_out_cpx[k].r + fft_out_cpx[k].i * fft_out_cpx[k].i) / norm;
        current_amps[k] = mag;
    }

    // 6. Analysis & State Update (Second Pass)
    int active_peak_count = 0;

    for (int k = 0; k < NUM_FREQS; k++) {
        FreqData* bin = &frq_array[k]; // Pointer to global state
        float new_amp = current_amps[k];
        float prev_amp = bin->amp_float;

        // Check Peak Status
        bin->is_peak = is_peak(current_amps, k, NUM_FREQS);

        if (bin->is_peak) {
            bin->env_phase = get_env_phase(new_amp, prev_amp);
            bin->stability++;

            if (bin->stability > STABILITY_COUNT) {
                bin->play = true;
                active_peak_count++;

                // --- Jitter Filter (Low Pass) ---
                // Smooths the target amplitude to prevent servo/magnet jitters
                float smoothed_target = (new_amp * 0.5f) + (prev_amp * 0.5f);
                bin->amp_float = smoothed_target;

                // Apply Output Gain & Clip
                float boosted = smoothed_target * AMP_CORRECTION_FACTOR;
                if (boosted > 1.0f) boosted = 1.0f;
                
                bin->amp = (int16_t)(boosted * 32767.0f);
            }
        } else {
            // Decay Logic
            bin->env_phase = 0;
            bin->stability = 0;
            bin->play = false;
            bin->amp = 0;
            // Immediate update for history
             bin->amp_float = new_amp;
        }
    }

    #ifdef DEBUG_ANALYSIS
    if (active_peak_count > 0) {
        printf(">> Peaks: %d\n", active_peak_count);
    }
    #endif
}