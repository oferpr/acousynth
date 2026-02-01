#include "input_config.hpp"
#include "hardware/clocks.h"
#include <stdio.h>

// --- Global Instance Definitions ---
FreqData frq_array[FFT_SIZE / 2];

// Double Buffers in RAM
int16_t input_buffer_1[HOP_SIZE];
int16_t input_buffer_2[HOP_SIZE];

// Pointers for Double Buffering
int16_t* active_adc_dma_buffer = input_buffer_2;
int16_t* inactive_adc_dma_buffer = input_buffer_1;

// Hardware Handles
int adc_dma_chan;
volatile bool new_data_ready = false;

// --- Helper Functions ---

static void init_input_buffers(void) {
    memset(input_buffer_1, 0, sizeof(input_buffer_1));
    memset(input_buffer_2, 0, sizeof(input_buffer_2));
}

// --- Driver Implementation ---

void adc_setup() {
    adc_init();
    init_input_buffers();
    
    // GPIO Setup
    adc_gpio_init(INPUT_PIN);
    adc_select_input(ADC_INPUT);

    // Stop ADC before config
    adc_run(false);

    // Clock Divider Calculation
    float clk_hz = (float)clock_get_hz(clk_adc);
    float clk_div = (clk_hz / FS_I) - 1.0f;
    adc_set_clkdiv(clk_div);

    // FIFO Setup
    adc_fifo_setup(
        true,  // Enable FIFO
        true,  // Enable DMA Request (DREQ)
        1,     // Threshold (Trigger on 1 sample)
        false, // No Error bit (We want clean 12-bit data)
        false  // No Shift (Keep 12-bit alignment)
    );
    
    printf("[ADC] Setup Complete. Clock Div: %.2f\n", clk_div);
}

void dma_init_setup() {
    // Claim Channel
    adc_dma_chan = dma_claim_unused_channel(true);
    
    // Config
    dma_channel_config c = dma_channel_get_default_config(adc_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, false); // Read from fixed FIFO
    channel_config_set_write_increment(&c, true); // Write to buffer
    channel_config_set_dreq(&c, DREQ_ADC);        // Paced by ADC
    
    // Apply Config
    dma_channel_configure(
        adc_dma_chan,
        &c,
        active_adc_dma_buffer, // Dest
        &adc_hw->fifo,         // Source
        HOP_SIZE,              // Count
        false                  // Don't start yet
    );

    // Setup Interrupts
    dma_channel_set_irq1_enabled(adc_dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_1, dma_isr);
    irq_set_enabled(DMA_IRQ_1, true);
}

// --- Critical Interrupt Service Routine ---
// Executed when DMA finishes filling a buffer (HOP_SIZE samples)
void dma_isr() {
    // 1. Clear Interrupt Flag
    dma_hw->ints1 = 1u << adc_dma_chan;

    // 2. Swap Buffers (Ping-Pong)
    // The 'active' buffer is now full, so it becomes 'inactive' (for analysis)
    // The 'inactive' buffer is empty, so it becomes 'active' (for DMA)
    int16_t* filled_buffer = active_adc_dma_buffer;
    active_adc_dma_buffer = inactive_adc_dma_buffer;
    inactive_adc_dma_buffer = filled_buffer;

    // 3. Restart DMA immediately
    // Point to the new empty buffer and reset counter
    dma_channel_set_write_addr(adc_dma_chan, active_adc_dma_buffer, false);
    dma_channel_set_trans_count(adc_dma_chan, HOP_SIZE, true); // Trigger now

    // 4. Notify Main Loop
    new_data_ready = true;
}