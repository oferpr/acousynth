#ifndef WAVETABLES_H
#define WAVETABLES_H

#include <stdint.h>
#include <math.h>
#include "pico/stdlib.h"
#include "macros.hpp" 

// --- 1. Global Wavetable Pointer ---
// The DMA reads from this pointer. We just change where it points.
extern int16_t *current_wave_table;

// --- 2. Function Prototypes ---

/**
 * @brief Generates the 4 base waveforms (Sine, Saw, Square, Triangle) 
 * and stores them in static memory. Must be called once at startup.
 */
void init_wavetables();

/**
 * @brief Mixes the 4 base waves into the master SYNTH_TABLE.
 * Weights are automatically normalized to prevent clipping.
 * * @param w_sine   Weight of Sine wave (0.0 - 1.0)
 * @param w_saw    Weight of Saw wave (0.0 - 1.0)
 * @param w_square Weight of Square wave (0.0 - 1.0)
 * @param w_tri    Weight of Triangle wave (0.0 - 1.0)
 */
void set_synth_table(float w_sine, float w_saw, float w_square, float w_tri);

/**
 * @brief Sets the envelope parameters. 
 * Currently hardcoded, but designed to take inputs later.
 * * @param attack_time   Pointer to write the attack time (ms)
 * @param release_time  Pointer to write the release time (ms)
 */
void set_synth_env(int16_t *attack_time, int16_t *release_time);

#endif /* WAVETABLES_H */