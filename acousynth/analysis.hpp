#ifndef ANALYSIS_H
#define ANALYSIS_H
#include "input_config.hpp"
#include <stdint.h>

#define AMP_CORRECTION_FACTOR 1.0f

/**
 *  Initializes the FFT, Hanning window, and processing buffers.
 */
void analysis_init();

/**
 * Processes a new buffer of audio samples.
 * This performs the full STFT pipeline:
 * 1. Shifts the internal processing buffer
 * 2. Adds the new normalized samples
 * 3. Applies a Hanning window
 * 4. Runs the FFT
 * 5. Analyzes peaks, stability, and envelope
 * 6. Updates the global `frq_array` with .play and .amp values.
 *
 *  Pointer to the newly filled DMA buffer (size: HOP_SIZE).
 */
void analyze_audio_segment(int16_t* new_samples);



#endif // ANALYSIS_H
