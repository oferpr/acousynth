// input.h

#ifndef INPUT_H
#define INPUT_H

#include "macros.hpp"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/irq.h"

#define INPUT_PIN 26
#define ADC_INPUT 0

typedef enum {
    ATTACK = 1,
    SUSTAIN = 0,
    RELEASE = -1
} Env_Phase;

// --- Frequency Data Structure ---
// Holds the state for a single frequency bin.
typedef struct FreqData{
    // --- Synthesis (DDS) Fields ---
    bool play;                  // True if this freq should be synthesized
    uint32_t accumalated_phase; // Current phase for the DDS oscillator
    uint32_t increment_j;       // Phase increment per sample, based on freq
    int16_t amp;                // Amplitude for DDS (Q15 format: 0-32767)
    float current_amp;              // Current amplitude (smoothed) value for synthesis

    // --- Analysis State Fields ---
    float amp_float;            // Previous frame's amp (normalized 0.0-1.0)
    bool is_peak;               // True if this bin was a local peak
    int env_phase;              // 1=attack, 0=sustain, -1=decay
    int stability;              // Counter for how long this peak has been stable
} FreqData;

// The global array linking analysis to synthesis
extern FreqData frq_array[FFT_SIZE / 2]; // HOP_SIZE is FFT_SIZE / 2


extern int16_t input_buffer_1[HOP_SIZE];
extern int16_t input_buffer_2[HOP_SIZE];

extern int16_t* active_adc_dma_buffer;
extern int16_t* inactive_adc_dma_buffer;
extern int adc_dma_chan;
extern volatile bool new_data_ready;

void dma_init_setup();
void adc_setup();
void adc_error_handler(int16_t *sample_ptr);
static void init_input_buffers();
void dma_isr();

#endif // INPUT_H
