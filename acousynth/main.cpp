/**
 * Project: Acousynth Live (RP2040 Real-Time Electro-Acoustic Synth)
 * File: main.cpp
 * Author: Ofer Pressman
 * Description: 
 * Core firmware entry point. Manages the high-speed DMA data acquisition 
 * from the ADC, spectral analysis (FFT), and I2S audio output.
 * * Architecture:
 * 1. ADC samples audio via DMA (Double Buffered).
 * 2. Main loop processes audio blocks and updates wavetables based on DDS algorithm.
 * 3. I2S Interface outputs synthesized audio with deterministic latency.
 */

#include "macros.hpp"
#include "input_config.hpp"
#include "output_config.hpp"
#include "analysis.hpp"
#include "wavetables.hpp"
#include "pico/audio_i2s.h"
#include "hardware/irq.h"
#include <stdio.h> 

// --- Configuration Constants ---
constexpr uint STATUS_LED_PIN = 15;
constexpr uint STARTUP_DELAY_MS = 2000;
constexpr int  NUM_PRIME_BUFFERS = 4;

// --- Control Loop Parameters ---
// Kp: Proportional gain for the amplitude envelope follower.
// Tuned for: Kp = 1/(FS_O * tau)
float Kp = 0.002f; 

// --- Main Application ---
int main() {
    // 1. System Initialization
    stdio_init_all();
    sleep_ms(STARTUP_DELAY_MS);
    printf("=== Acousynth Live System Starting ===\n");
    
    // Initialize Subsystems
    init_wavetables();
    set_synth_table(); // Loads default sine/saw/square tables
    increment_init();
    analysis_init();
    
    // 2. Hardware Setup
    // Configure ADC to feed the DMA buffer
    adc_setup();
    printf("[System] ADC Input Configured\n");
    
    // Configure DMA for zero-copy transfer
    dma_init_setup();
    printf("[System] ADC-DMA Configured\n");
    
    // Enable DMA Interrupts
    irq_set_exclusive_handler(DMA_IRQ_1, dma_isr);
    irq_set_enabled(DMA_IRQ_1, true);
    
    // Configure I2S Output (PIO based)
    set_i2s();
    connect_o_buffers();
    printf("[System] Audio Output Configured\n");

    // 3. Buffer Priming (Crucial for I2S Stability)
    // We manually fill the buffer pool BEFORE starting the hardware.
    // This prevents the I2S engine from reading empty memory (underflow) at startup.
    printf("[System] Priming Audio Buffers...\n");
    for (int i = 0; i < NUM_PRIME_BUFFERS; i++) {
        fetch_o_samples(Kp);
    }

    // 4. Critical Startup Sequence
    // Order matters: DMA must be listening before ADC starts firing.
    dma_channel_start(adc_dma_chan); // 1. Arm DMA
    adc_run(true);                   // 2. Start ADC
    audio_i2s_set_enabled(true);     // 3. Start I2S Clock
    
    printf("[System] Real-Time Loop Running...\n");

    // Setup Status LED
    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    uint32_t last_blink_time = 0;

    // --- 5. Main Real-Time Loop ---
    while (true) {
        // A. Audio Synthesis
        // Generates the next block of audio samples based on current state.
        fetch_o_samples(Kp); 

        // B. Spectral Analysis (Event Driven)
        // If the DMA has filled a new input buffer (new_data_ready), process it.
        // This runs asynchronously to the audio generation.
        if (new_data_ready) {
            new_data_ready = false; 
            
            // Perform FFT and update synth parameters
            analyze_audio_segment(inactive_adc_dma_buffer);
        }

        // C. Heartbeat LED (Non-blocking)
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_blink_time > 500) {
            gpio_put(STATUS_LED_PIN, !gpio_get(STATUS_LED_PIN));
            last_blink_time = now;
        }
    }
    return 0;
}