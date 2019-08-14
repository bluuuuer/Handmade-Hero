#if !defined(HANDEMADE_H)

#include <stdint.h>
#include <math.h>


typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float real32;
typedef double real64;

#define Pi32 3.1415926f

#define local_persist static

struct game_offscreen_buffer {
    void *memory;
    int width;
    int height;
    int pitch;
};

struct game_sound_output_buffer {
    int sampleCount;
    int samplesPerSecond;
    int16 *samples;
};

/*
- timing
- controller/keyboard input
- bitmap buffer to use
- sound buffer to use
 */
void GameUpdateAndRender(game_offscreen_buffer *buffer, int blueOffset, int greenOffset, 
                         game_sound_output_buffer *soundBuffer, int toneHZ);

#define HANDMADE_H
#endif