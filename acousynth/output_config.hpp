#ifndef OUTPUT_CONFIG_H
#define OUTPUT_CONFIG_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "wavetables.hpp" // Assumed to contain your wavetable definitions
#include "hardware/irq.h"
#include "pico/time.h"
#include "input_config.hpp"
#include "pico/audio.h"
#include "pico/sample_conversion.h"

// --- I2S Configuration ---
#define I2S_DATA_PIN 9
#define I2S_CLOCK_PIN_BASE 10 // BCLK on 10, LRCLK on 11
#define O_DMA_CHANNEL 10
#define PIO_NUM 0

// --- Audio Configuration ---
#define FS_O 44100
#define O_BUFFER_SIZE 256 // Number of samples to generate at a time. A good starting point.

// --- Global variables for pico_audio ---
extern audio_format_t audio_format;
extern audio_i2s_config_t i2s_config;
extern audio_format_t *output_format;
extern audio_buffer_format_t output_buffer_format;
extern audio_buffer_pool_t *output_pool;

void increment_init();
void set_i2s();
void connect_o_buffers();
void fill_o_buffer(audio_buffer_t *buffer, float Kp);
void fetch_o_samples(float Kp);

#endif // OUTPUT_CONFIG_H
