#include "input_config.hpp"
#include <stdio.h>

// Define the globals here
FreqData frq_array[FFT_SIZE / 2];
int16_t input_buffer_1[HOP_SIZE];
int16_t input_buffer_2[HOP_SIZE];
int16_t* active_adc_dma_buffer = input_buffer_2;
int16_t* inactive_adc_dma_buffer = input_buffer_1;
int adc_dma_chan;
volatile bool new_data_ready = false;

//initialize buffers
void init_input_buffers(void) {
    for (int i = 0; i < HOP_SIZE; i++) {
        input_buffer_1[i] = 0;
        input_buffer_2[i] = 0;
    }
}

void adc_setup() {
    adc_init();
    init_input_buffers();
    adc_gpio_init(INPUT_PIN);
    adc_select_input(ADC_INPUT);

    adc_run(false);

    adc_set_clkdiv ((float)((48000000 / FS_I) - 1));
    adc_fifo_setup(
        true,  // Enable FIFO
        true,  // Enable DREQ
        1,     // Threshold
        false, // ERR_IN_FIFO: Set to FALSE. We want pure audio, not error flags in our data.
        false  // Byte shift
    );
    adc_hw->cs = adc_hw->cs | 0x00000100; 
    adc_fifo_drain();

    printf("ADC Clean Status: 0x%x\n", adc_hw->cs);
}


void adc_error_handler(int16_t *sample_ptr) { //callback when bit 15 of the FIFO sample contains error flag
    int16_t lag_sample = *(sample_ptr - 1); //
    int16_t lead_sample = *(sample_ptr + 1); //
    *sample_ptr = (int16_t)((lag_sample + lead_sample) >> 2); //linear approx b = (a+c)/2 - CAN BE MOR ACCURATE USING TAYLOR SERIES OF SIN/COS
}

void dma_isr() {
    dma_hw->ints1 = 1u << adc_dma_chan; //clear the interrupt request
    //swap buffers
    int16_t* temp = active_adc_dma_buffer; //swap buffers
    active_adc_dma_buffer = inactive_adc_dma_buffer;
    inactive_adc_dma_buffer = temp;

    dma_channel_set_write_addr(adc_dma_chan, active_adc_dma_buffer, false); //Reset the pointer (do trigger yet, pass false)
    dma_channel_set_trans_count(adc_dma_chan, HOP_SIZE, true); // Reset the counter AND Trigger (pass true)

    new_data_ready = true; //set flag for main loop

}

void dma_init_setup() {
    // Get a free channel, panic() if there are none
    adc_dma_chan = dma_claim_unused_channel(true);
    
    //--- DMA Config---
    // 8 bit transfers. Both read and write address increment after each
    // transfer (each pointing to a location in src or dst respectively).
    // No DREQ is selected, so the DMA transfers as fast as it can.
    dma_channel_config c = dma_channel_get_default_config(adc_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq (&c, DREQ_ADC);
    dma_channel_configure(
        adc_dma_chan,          // Channel to be configured
        &c,            // The configuration we just created
        active_adc_dma_buffer,           // The initial write address
        &adc_hw->fifo,           // The initial read address
        HOP_SIZE, // Number of transfers; in this case each is 1 byte.
        false           // doesn't start dma
    );
    // Tell the DMA to raise IRQ 1 when this channel finishes
    dma_channel_set_irq1_enabled(adc_dma_chan, true);
    
    // Set our ISR function as the handler for DMA IRQ 1
    irq_set_exclusive_handler(DMA_IRQ_1, dma_isr);
}

