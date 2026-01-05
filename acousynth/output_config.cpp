#include "output_config.hpp"
#include "macros.hpp"
#include "input_config.hpp"
#include "analysis.hpp"

audio_format_t audio_format;
audio_i2s_config_t i2s_config;
audio_buffer_format_t output_buffer_format;
audio_format_t *output_format;
audio_buffer_pool_t *output_pool;

void increment_init() {
    // 3. Initialize the Frequency Array map
    float freq_resolution = (float)FS_I / (float)FFT_SIZE; // Frequency resolution of each bin
    
    for (int k = 0; k < NUM_FREQS; k++) {
        float freq_hz = k * freq_resolution;
        double inc = (freq_hz / (double)FS_O) * 4294967296.0; // 2^32 = 4294967296
        
        frq_array[k].increment_j = (uint32_t)inc;
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

void fill_o_buffer(audio_buffer_t *buffer, float Kp = 0.01f) {
    //initilize output buffer
    int16_t *samples = (int16_t *) buffer->buffer->bytes; // Pointer to the buffer's sample data
    
    // 1. Temporary 32-bit buffer to prevent overflow during mixing
    // We use 'static' to avoid re-allocating it every interrupt (faster)
    // O_BUFFER_SIZE is small (256), so this fits easily in RAM.
    static int32_t mix_buffer[O_BUFFER_SIZE];
    
    // Clear the mix buffer
    memset(mix_buffer, 0, buffer->max_sample_count * sizeof(int32_t));
    
    // Safety check on the new static pointer
    if (!current_wave_table) return;

    // Outer loop: Iterate through active frequencies - cache-efficient
    for (int j = 0; j < NUM_FREQS; j++) {
        if (frq_array[j].play || frq_array[j].current_amp > 1.0f) { // RELEASE: We process this frequency if it is meant to play OR if it's still fading out. We stop only when it's effectively silent (< 1.0).
            uint32_t ap = frq_array[j].accumalated_phase; // Load phase once
            uint32_t inc = frq_array[j].increment_j;       // Load increment once
            
            // If play is false, target is 0 (Fade out). 
            // If play is true, target is the detected amp.
            int16_t target_amp = frq_array[j].play ? frq_array[j].amp : 0;
            float current_amp = frq_array[j].current_amp;

            // Inner loop: Generate all samples for this one frequency
            for (uint i = 0; i < buffer->max_sample_count; i++) {
                float error = (float)target_amp - current_amp;
                current_amp += error * Kp;

                // FIX: SAFETY MASK
                // We use the mask to ensure the index wraps around correctly.
                // This prevents reading garbage memory if ap overflows weirdly.
                uint32_t table_index = (ap >> PHASE_SHIFT) & WAVETABLE_MASK;
                
                int16_t table_sample = current_wave_table[table_index];

                // Standard Mix with headroom
                int32_t product = ((int32_t)table_sample * (int16_t)current_amp) >> 2;

                // Add this frequency's contribution to the sample
                // This requires clipping at the end
                mix_buffer[i] += (product >> 15);   // Q15 format adjustment

                ap += inc; // Advance phase
            }
            // Store the final phase back
            frq_array[j].accumalated_phase = ap;
            frq_array[j].current_amp = current_amp;
        }
    }
    
    // --- 2. POST PROCESSING (Simple Hard Clamp) ---
    // We are removing the cubic math to rule it out as the source of "squashed" sound.
    for (uint i = 0; i < buffer->max_sample_count; i++) {
        int32_t val = mix_buffer[i];
        
        // Simple Hard Limiter
        if (val > 32767) val = 32767;
        else if (val < -32767) val = -32767;

        int16_t out_val = (int16_t)val;

        // Write to Stereo
        samples[i*2]     = out_val;
        samples[i*2 + 1] = out_val;
    }

    buffer->sample_count = buffer->max_sample_count;
}


void set_i2s() {
    // --- Setup I2S Driver ---
    // define the audio format for output
    audio_format.format = AUDIO_BUFFER_FORMAT_PCM_S16; //audio format
    audio_format.sample_freq = FS_O; //out sample rate
    audio_format.channel_count = 2; //Stereo

    // define  I2S configuration
    i2s_config.data_pin = I2S_DATA_PIN; // data pin
    i2s_config.clock_pin_base = I2S_CLOCK_PIN_BASE; //BCK (base clock pin);
    i2s_config.dma_channel = O_DMA_CHANNEL; // DMA channel 1
    i2s_config.pio_sm =  PIO_NUM;   // PIO 0, State Machine 0

    // Initialize the I2S driver with our configuration
    const audio_format_t *output_format = audio_i2s_setup(&audio_format, &i2s_config);
    if (!output_format) {
        panic("Pico Audio I2S failed to setup!");
    }
}

void connect_o_buffers() {
    // --- 2. Create and Connect the Producer (Your Synthesizer) ---\
    //define th buffer's format
    output_buffer_format.format = &audio_format; //audio format
    output_buffer_format.sample_stride = 4; //sample stride - no. bytes - For S16 stereo, each sample is 4 bytes (16 bits * 2 channels)

    // The producer pool is where our synthesizer will place the audio buffers it generates.
    // We'll create a pool of 3 buffers.
    output_pool = audio_new_producer_pool(&output_buffer_format, 3, O_BUFFER_SIZE);

    // Connect the producer pool to the I2S driver.
    // The driver will now automatically take buffers from this pool for playback.
    if (!audio_i2s_connect(output_pool)) {
        panic("Failed to connect audio producer pool");
    }
}

void fetch_o_samples(float Kp) {
    // Get a free buffer from the producer pool. This will block until one is available.
    audio_buffer_t *buffer = take_audio_buffer(output_pool, false);

    if (buffer == NULL) {
        // The audio system is backed up (or broken). 
        // We return immediately so we don't hang the Analysis loop.
        return; 
    }

    // Generate audio data and fill the buffer.
    fill_o_buffer(buffer, Kp);

    // "Give" the filled buffer back to the pool. The I2S driver will now pick it up for DMA transfer.
    give_audio_buffer(output_pool, buffer);
}