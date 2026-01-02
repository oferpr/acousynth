#include "macros.hpp"
#include "input_config.hpp"
#include "output_config.hpp"
#include "analysis.hpp"
#include "wavetables.hpp"
#include "pico/audio_i2s.h"
#include "hardware/irq.h"
#include <stdio.h> // for printf

//--- TEST FUNCTIONS---
void inject_test_tone(int16_t* buffer, float freq, float sample_rate) {
    static float phase = 0;
    for (int i = 0; i < HOP_SIZE; i++) {
        // Generate sine wave scaled to 12-bit ADC range (0-4095)
        // Center at 2048, Amplitude ~1000
        float val = 2048.0f + 1000.0f * sinf(phase);
        
        buffer[i] = (int16_t)val;
        
        phase += (2.0f * M_PI * freq) / sample_rate;
        if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("=== Acousynth Live ===\n");
    
    init_wavetables();
    set_synth_table(sine_wave, 1.0, saw_wave, 0.0, square_wave, 0.0);
    increment_init();
    analysis_init();
    
    // 1. SETUP ADC
    adc_setup();
    printf("[System] ADC Input Configured\n");
    
    // 2. SETUP DMA
    dma_init_setup();
    printf("[System] ADC-DMA Configured\n");
    
    irq_set_exclusive_handler(DMA_IRQ_1, dma_isr);
    irq_set_enabled(DMA_IRQ_1, true);
    
    // 3. SETUP AUDIO OUTPUT (Uncommented!)
    set_i2s();
    connect_o_buffers();
    printf("[System] Audio Output Configured\n");

    // We manually fill the buffer pool BEFORE starting the hardware.
    // This prevents the I2S engine from starving/crashing immediately.
    printf("[System] Priming Audio Buffers...\n");
    for (int i = 0; i < 4; i++) {
        fetch_o_samples();
    }

    // 4. STARTUP SEQUENCE (Strict Order)
    dma_channel_start(adc_dma_chan); // Start DMA first (it waits)
    adc_run(true);                   // Start ADC (feeds DMA)
    audio_i2s_set_enabled(true);     // Start Audio Output
    
    uint32_t last_blink_time = 0;
    
    printf("[System] Running...\n");

    gpio_init(15);
    gpio_set_dir(15, GPIO_OUT);
    // --- 5. Main Application Loop ---
    // The main loop is now responsible for generating audio data and updating synth parameters.
    
    // For TEST
    //int n = 0;
    //uint32_t last_pitch_time = 0;
    
    while (true) {
        fetch_o_samples();

        // Blink LED every 500ms to prove the loop is running
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_blink_time > 500) {
            gpio_put(15, !gpio_get(15));
            last_blink_time = now;
        }

        // The `frq_array` can be updated here based on external events (MIDI, UI, etc.)
        // This can happen at your slower rate (~204 ms) without interrupting the audio stream.
        // For example: check for new notes to play/stop.
        if (new_data_ready) {
            new_data_ready = false; // reset flag

            // --- DEBUG MODE: OVERWRITE REAL DATA ---
            // Uncomment this line to test perfect 110Hz (A string)
            /*inject_test_tone(inactive_adc_dma_buffer, 110.0f * (pow(-1.0f, n) + 2.0f), FS_I);
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (now - last_pitch_time > 5000) {
                n++;
                last_pitch_time = now;
            }*/

            printf("analyzing! ADC Value: %d\n", inactive_adc_dma_buffer[0]);
            analyze_audio_segment(inactive_adc_dma_buffer);
            
        }
        
    }
    return 0;
}