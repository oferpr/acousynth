/**
 * File: wavetables.cpp
 * Description: Generates base waveforms and handles additive mixing.
 */

#include "wavetables.hpp"
#include "macros.hpp" // Ensure WAVETABLE_LEN and TWO_PI are here
#include <math.h>
#include <stdio.h>
#include <string.h>   // for memset

// --- 1. STATIC MEMORY ALLOCATION ---
// We allocate these in BSS (RAM) to avoid Heap fragmentation.
// These hold the "Source" data.
int16_t SINE_TABLE[WAVETABLE_LEN];
int16_t SAW_TABLE[WAVETABLE_LEN];
int16_t SQUARE_TABLE[WAVETABLE_LEN];
int16_t TRI_TABLE[WAVETABLE_LEN];

// This holds the "Destination" (Mixed) data that the DMA reads.
int16_t SYNTH_TABLE[WAVETABLE_LEN];

// Pointer exposed to main.cpp
int16_t *current_wave_table = SINE_TABLE; 

// --- 2. GENERATION LOGIC ---
void init_wavetables() {
    printf("[Wavetables] Generating Base Tables (Len: %d)...\n", WAVETABLE_LEN);

    for (int i = 0; i < WAVETABLE_LEN; i++) {
        // A. Sine Wave (Standard)
        // sin(0..2PI) -> -1.0 to 1.0
        float v_sine = sinf((float)TWO_PI * i / WAVETABLE_LEN);
        SINE_TABLE[i] = (int16_t)(v_sine * 32767.0f);

        // B. Sawtooth Wave (Ramp Down)
        // Goes from 1.0 to -1.0
        float v_saw = 1.0f - (2.0f * (float)i / (float)WAVETABLE_LEN);
        SAW_TABLE[i] = (int16_t)(v_saw * 32767.0f);

        // C. Square Wave (50% Duty Cycle)
        // High for first half, Low for second half
        if (i < WAVETABLE_LEN / 2) {
            SQUARE_TABLE[i] = 32767;
        } else {
            SQUARE_TABLE[i] = -32767;
        }

        // D. Triangle Wave
        // Rises to 1.0, falls to -1.0
        float phase = (float)i / (float)WAVETABLE_LEN;
        float v_tri = 0.0f;
        if (phase < 0.5f) {
            v_tri = -1.0f + (4.0f * phase); // Rise
        } else {
            v_tri = 3.0f - (4.0f * phase);  // Fall
        }
        TRI_TABLE[i] = (int16_t)(v_tri * 32767.0f);
    }
    
    // Set default to pure sine to start
    set_synth_table(1.0f, 0.0f, 0.0f, 0.0f);
}

// --- 3. ADDITIVE SYNTHESIS MIXER ---
// Mixes the 4 base tables into SYNTH_TABLE with normalization.
void set_synth_table(float w_sine, float w_saw, float w_square, float w_tri) {
    
    // 1. Calculate Total Weight for Normalization
    // We must normalize to prevent clipping (overflowing int16).
    float total_weight = w_sine + w_saw + w_square + w_tri;
    
    // Safety: Prevent divide by zero if user sends all 0.0s
    if (total_weight < 0.0001f) {
        total_weight = 1.0f; 
        // Optional: Could zero out table here, but let's keep logic simple
    }

    float normalization_factor = 1.0f / total_weight;

    // 2. Additive Mixing Loop
    for (int i = 0; i < WAVETABLE_LEN; i++) {
        // Weighted Sum (using floats for precision)
        float mixed_sample = (SINE_TABLE[i]   * w_sine) +
                             (SAW_TABLE[i]    * w_saw)  +
                             (SQUARE_TABLE[i] * w_square) +
                             (TRI_TABLE[i]    * w_tri);
        
        // Normalize back to range
        mixed_sample *= normalization_factor;

        // Store in Destination Table
        SYNTH_TABLE[i] = (int16_t)mixed_sample;
    }

    // 3. Point the engine to the new mixed table
    current_wave_table = SYNTH_TABLE;
}

void set_synth_env(int16_t *attack_time, int16_t *release_time) {
    *attack_time = 30;   // ms
    *release_time = 100; // ms
}