#include "output_config.hpp"
#include "macros.hpp"
#include "analysis.hpp" // For frq_array access
#include "wavetables.hpp" // For current_wave_table access
#include <string.h>     // For memset

// --- Internal Driver State ---
static audio_format_t audio_format;
static audio_i2s_config_t i2s_config;
static audio_buffer_format_t output_buffer_format;
static audio_buffer_pool_t *output_pool;

// --- 1. Initialization Logic ---

void increment_init() {
    printf("[Synth] Initializing Phase Increments...\n");
    
    // Frequency resolution of the FFT bins based on Input Sample Rate
    // Note: FS_I is low (1255 Hz), so bins are very fine (~2.4 Hz).
    float freq_resolution = (float)FS_I / (float)FFT_SIZE; 
    
    // Direct Digital Synthesis (DDS) Constant: 2^32 / Fs_out
    // Maps a target Hz value to a 32-bit phase step per sample.
    double dds_factor = two32 / (double)FS_O;

    for (int k = 0; k < NUM_FREQS; k++) {
        float freq_hz = k * freq_resolution;
        
        // Calculate phase increment for this specific bin frequency
        frq_array[k].increment_j = (uint32_t)(freq_hz * dds_factor);
        
        // Clear state
        frq_array[k].play = false;
        frq_array[k].accumalated_phase = 0;
        frq_array[k].amp = 0;
        frq_array[k].current_amp = 0.0f;
        frq_array[k].amp_float = 0.0f;
        frq_array[k].is_peak = false;
        frq_array[k].env_phase = 0;
        frq_array[k].stability = 0;
    }
}

void set_i2s() {
    // Define format: 16-bit Stereo @ 44.1kHz
    audio_format.format = AUDIO_BUFFER_FORMAT_PCM_S16;
    audio_format.sample_freq = FS_O;
    audio_format.channel_count = 2;

    // Configure PIO Pins
    i2s_config.data_pin = I2S_DATA_PIN;
    i2s_config.clock_pin_base = I2S_CLOCK_PIN_BASE;
    i2s_config.dma_channel = O_DMA_CHANNEL;
    i2s_config.pio_sm = PIO_NUM;

    // Initialize Pico Audio Driver
    const audio_format_t *ret = audio_i2s_setup(&audio_format, &i2s_config);
    if (!ret) {
        panic("Pico Audio I2S Setup Failed!");
    }
}

void connect_o_buffers() {
    // Define Buffer Format (S16 Stereo = 4 bytes per sample)
    output_buffer_format.format = &audio_format;
    output_buffer_format.sample_stride = 4;

    // Create Pool: 3 buffers of size O_BUFFER_SIZE
    // 3 buffers allow: [1 Playing] [1 Ready] [1 Being Filled]
    output_pool = audio_new_producer_pool(&output_buffer_format, 3, O_BUFFER_SIZE);

    if (!audio_i2s_connect(output_pool)) {
        panic("Failed to connect I2S producer pool");
    }
}

// --- 2. Synthesis Engine (The Hot Path) ---

// Internal helper to mix samples
static void fill_o_buffer(audio_buffer_t *buffer, float Kp) {
    int16_t *samples = (int16_t *)buffer->buffer->bytes;
    
    // High-precision mixing buffer (32-bit to prevent overflow before clipping)
    // Static allocation avoids stack thrashing
    static int32_t mix_buffer[O_BUFFER_SIZE];
    
    // Reset mix buffer
    memset(mix_buffer, 0, buffer->max_sample_count * sizeof(int32_t));
    
    // Safety: Don't run if wavetable isn't ready
    if (!current_wave_table) return;

    // --- A. Additive Synthesis Loop ---
    for (int j = 0; j < NUM_FREQS; j++) {
        // Optimization: Skip silent frequencies
        // We also check 'current_amp > 1.0' to ensure we process the full decay tail
        if (!frq_array[j].play && frq_array[j].current_amp <= 1.0f) {
            continue;
        }

        // Load Frequency State
        uint32_t ap = frq_array[j].accumalated_phase;
        uint32_t inc = frq_array[j].increment_j;
        
        // Envelope Follower Logic
        // Target is either the live amplitude (if playing) or 0 (if stopped)
        int16_t target_amp = frq_array[j].play ? frq_array[j].amp : 0;
        float current_amp = frq_array[j].current_amp;

        // Sample Generation Loop
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            // 1. P-Control Envelope Smoothing
            float error = (float)target_amp - current_amp;
            current_amp += error * Kp;

            // 2. WaveTable Lookup
            // Use top bits of phase accumulator for index
            // With PHASE_SHIFT=22 (32-10), we correctly map 32-bit phase to 1024 table
            uint32_t table_index = (ap >> PHASE_SHIFT) & WAVETABLE_MASK;
            int16_t wave_sample = current_wave_table[table_index];

            // 3. Apply Amplitude (Volume)
            // (Sample * Amp) >> 4 gives us headroom before final mix
            int32_t product = ((int32_t)wave_sample * (int16_t)current_amp) >> 2;

            // 4. Accumulate into Mix Buffer (Q15 adjustment)
            mix_buffer[i] += (product >> 15);

            // 5. Advance Phase
            ap += inc;
        }

        // Save State for next block
        frq_array[j].accumalated_phase = ap;
        frq_array[j].current_amp = current_amp;
    }
    
    // --- B. Final Output Stage ---
    for (uint i = 0; i < buffer->max_sample_count; i++) {
        int32_t val = mix_buffer[i];
        
        // Hard Clipper / Limiter
        if (val > 32767) val = 32767;
        else if (val < -32767) val = -32767;

        int16_t out_val = (int16_t)val;

        // Interleave to Stereo (Left = Right)
        samples[i*2]     = out_val;
        samples[i*2 + 1] = out_val;
    }

    buffer->sample_count = buffer->max_sample_count;
}

void fetch_o_samples(float Kp) {
    // Request free buffer (Non-blocking mode)
    audio_buffer_t *buffer = take_audio_buffer(output_pool, false);

    if (buffer == NULL) {
        // Buffer starvation occurred (CPU too slow or I2S too fast)
        return; 
    }

    fill_o_buffer(buffer, Kp);

    give_audio_buffer(output_pool, buffer);
}