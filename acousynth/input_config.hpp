/**
 * File: input_config.hpp
 * Description: ADC & DMA Driver Configuration.
 * Manages the high-speed data acquisition pipeline using Double Buffering.
 */

#ifndef INPUT_H
#define INPUT_H

#include "macros.hpp"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/irq.h"

// --- Hardware Pins ---
constexpr uint INPUT_PIN = 26;
constexpr uint ADC_INPUT = 0; // Maps to GPIO 26

// --- Data Structures ---

typedef enum {
    ATTACK = 1,
    SUSTAIN = 0,
    RELEASE = -1
} Env_Phase;

// Shared State for Synthesis & Analysis
typedef struct FreqData{
    // Synthesis Fields (Read by Output, Written by Analysis)
    bool play;                  // Gate flag
    uint32_t accumalated_phase; // DDS Phase Accumulator
    uint32_t increment_j;       // DDS Phase Step
    int16_t amp;                // Target Amplitude (Q15)
    float current_amp;          // Smoothed Amplitude (for envelope)

    // Analysis Fields (Read/Written by Analysis)
    float amp_float;            // History for jitter filter
    bool is_peak;               // Debug/Vis flag
    int env_phase;              // Envelope state
    int stability;              // Debounce counter
} FreqData;

// Global Accessors
extern FreqData frq_array[FFT_SIZE / 2];

// DMA Double Buffers
extern int16_t input_buffer_1[HOP_SIZE];
extern int16_t input_buffer_2[HOP_SIZE];

// Buffer Pointers (Swapped in ISR)
extern int16_t* active_adc_dma_buffer;
extern int16_t* inactive_adc_dma_buffer;

extern int adc_dma_chan;
extern volatile bool new_data_ready;

// --- Function Prototypes ---
void adc_setup();
void dma_init_setup();
void dma_isr();

#endif // INPUT_H