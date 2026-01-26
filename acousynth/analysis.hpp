/**
 * File: analysis.hpp
 * Description: Spectral analysis module using KissFFT.
 * Performs STFT (Short-Time Fourier Transform), peak detection, 
 * and envelope following to drive the synthesizer.
 */

#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "input_config.hpp"
#include <stdint.h>

// Adjust this to boost quiet signals from the guitar
#define AMP_CORRECTION_FACTOR 1.5f 

/**
 * @brief Initializes the FFT engine, Hanning window, and buffers.
 * Must be called before the main loop.
 */
void analysis_init();

/**
 * @brief Processes a new buffer of audio samples.
 * Pipeline:
 * 1. Overlap-Add (Sliding Window)
 * 2. Windowing (Hanning)
 * 3. FFT (Real-to-Complex)
 * 4. Peak Detection & Stability Check
 * 5. Parameter Mapping (Updates global frq_array)
 * * @param new_samples Pointer to the DMA buffer (size: HOP_SIZE)
 */
void analyze_audio_segment(int16_t* new_samples);

#endif // ANALYSIS_H