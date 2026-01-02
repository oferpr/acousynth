#ifndef MACROS_H
#define MACROS_H

#include <stdint.h>
#include <stdbool.h>
#include <cstdint>

#define FS_O 44100
#define O_BUFFER_SIZE 256

#define WAVETABLE_BITS 10 //log(TABLE_SIZE=O_BUFFERSIZE)
const int PHASE_SHIFT = 32 - WAVETABLE_BITS;
#define WAVETABLE_LEN 1024

#define TWO_PI 6.28318530718
#define two32 4294967296.0

#define FS_I 1255
#define HOP_SIZE 64//256 //BUFFER_SIZE/2
#define FFT_SIZE 512
#define I_BUFFER_SIZE FFT_SIZE
#define NUM_FREQS (FFT_SIZE / 2)


#endif /* MACROS_H */