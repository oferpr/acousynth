/**
 * File: output_config.hpp
 * Description: Interface for the I2S Audio Output Subsystem.
 */

#ifndef OUTPUT_CONFIG_H
#define OUTPUT_CONFIG_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "macros.hpp" // Provides FS_O and O_BUFFER_SIZE

// --- Hardware Configuration ---
// These are specific to the Output implementation
constexpr uint I2S_DATA_PIN       = 9;
constexpr uint I2S_CLOCK_PIN_BASE = 10; // BCLK on 10, LRCLK on 11
constexpr uint O_DMA_CHANNEL      = 10;
constexpr uint PIO_NUM            = 0;

// --- Public API ---

/**
 * @brief Calculates the DDS phase increments for the synthesis engine.
 * Must be called once at startup.
 */
void increment_init();

/**
 * @brief Configures the RP2040 I2S PIO driver and DMA channel.
 */
void set_i2s();

/**
 * @brief Allocates buffer pools and links them to the I2S DMA.
 */
void connect_o_buffers();

/**
 * @brief Main audio generation task. 
 * Fetches a free buffer, performs additive synthesis, and hands it to DMA.
 * * @param Kp Proportional gain for the envelope follower (P-Controller).
 */
void fetch_o_samples(float Kp);

#endif // OUTPUT_CONFIG_H