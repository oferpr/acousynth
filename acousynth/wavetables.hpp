#ifndef WAVETABLES_H
#define WAVETABLES_H

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "macros.hpp"
#include <cstdint>

typedef enum {
    SINE = 1,
    SAW = 2,
    SQUARE = 3,
    TRIANGLE = 4,
    SYNTH = 5
} WaveType;

typedef struct WaveTable{
    WaveType wave;
    int16_t table[WAVETABLE_LEN];
} WaveTable;

extern WaveTable *sine_wave;
extern WaveTable *saw_wave;
extern WaveTable *square_wave;
extern WaveTable *triangle_wave;
extern WaveTable *synth_wave;

extern int32_t *attack_time;
extern int32_t *release_time;

void init_wavetables();
void set_synth_table(WaveTable *wave1, float vol1, WaveTable *wave2, float vol2, WaveTable *wave3, float vol3);

#endif /* WAVETABLES_H */