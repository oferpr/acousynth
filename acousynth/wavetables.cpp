#include "wavetables.hpp"
#include "macros.hpp"
#include <math.h>
#include <stdio.h>


WaveTable *sine_wave;
WaveTable *saw_wave;
WaveTable *square_wave;
WaveTable *triangle_wave;
WaveTable *synth_wave;

void init_wavetables() {
    printf("[Output] Generating Wavetables...\n");
    sine_wave = new WaveTable();
    saw_wave = new WaveTable();
    square_wave = new WaveTable();
    triangle_wave = new WaveTable();
    sine_wave->wave = SINE;
    saw_wave->wave = SAW;
    square_wave->wave = SQUARE;
    triangle_wave->wave = TRIANGLE;

    for (int i = 0; i < WAVETABLE_LEN; i++) {
        // Sine wave
        sine_wave->table[i] = (int16_t)(sin((TWO_PI * i / WAVETABLE_LEN)) * INT16_MAX);
        // Sawtooth wave
        saw_wave->table[i] = (int16_t)((1 - 2 * ((float)i / WAVETABLE_LEN)) * INT16_MAX); //Ramp from 1 to -1
        // Square wave
        square_wave->table[i] = (i < WAVETABLE_LEN / 2) ? INT16_MAX : INT16_MIN; //True->1, False->-1
        // Triangle wave
        float phase = (float)i / WAVETABLE_LEN;
        triangle_wave->table[i] = (int16_t)((2.0f * fabs(2.0f * (phase - 0.5f)) - 1.0f) * INT16_MAX); //Triangle wave: 2.0 * abs(2.0 * (phase - 0.5)) - 1.0
    }
    synth_wave = sine_wave; //default
}

//[ms]. LATER - READ KNOBS DATA
// Mixes 3 waves into the 'synth_wave' table
void set_synth_table(WaveTable *wave1, float vol1, WaveTable *wave2, float vol2, WaveTable *wave3, float vol3) {
    // If synth_wave was pointing to a static wave (like sine_wave), allocate new memory
    // If it was already allocated as SYNTH, we can overwrite it.
    if (synth_wave == sine_wave || synth_wave == NULL) {
         synth_wave = new WaveTable();
    }
    
    synth_wave->wave = SYNTH;
    float total_vol = vol1 + vol2 + vol3;
    if (total_vol == 0.0f) total_vol = 1.0f; // Prevent divide by zero

    for (int i = 0; i < WAVETABLE_LEN; i++) {
        float mixed_val = (wave1->table[i] * vol1 + wave2->table[i] * vol2 + wave3->table[i] * vol3) / total_vol;
        synth_wave->table[i] = (int16_t)mixed_val;
    }
}
/*
void set_synth_table(WaveTable *wave1, float vol1, WaveTable *wave2, float vol2, WaveTable *wave3, float vol3) {
    synth_wave = new WaveTable();
    synth_wave->wave = SYNTH;

    for (int i = 0; i < WAVETABLE_LEN; i++) {
        synth_wave->table[i] = (wave1->table[i] * vol1 + wave2->table[i] * vol2 + wave3->table[i] * vol3) / (vol1 + vol2 + vol3);
    }
}
*/

void set_synth_env(int16_t *attack_time, int16_t *release_time) {
    *attack_time = 30; //[ms]. LATER - READ KNOB DATA
    *release_time = 100; //[ms]. LATER - READ KNOB DATA
}
