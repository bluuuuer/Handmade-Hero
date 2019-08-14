#include "handmade.h"

void GameOutputSound(game_sound_output_buffer *soundBuffer, int toneHz) {
    local_persist real32 tSine;
    int16 toneVolume = 2000;
    int wavePeriod = soundBuffer->samplesPerSecond / toneHz;

    int16 *sampleOut = soundBuffer->samples;
    for (DWORD sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; ++ sampleIndex) {
        real32 sineValue = sinf(tSine);
        int16 sampleValue = (int16)(sineValue * toneVolume);
        *sampleOut ++ = sampleValue;
        *sampleOut ++ = sampleValue;
        tSine += 2.0f * Pi32 * 1.0f / (real32)wavePeriod;
    }
}

void RenderGradient(game_offscreen_buffer *buffer, int xOffset, int yOffset) {
    uint8 *row = (uint8 *)buffer->memory;
    for (int i = 0; i < buffer->height; i ++) {
        uint32 *pixel = (uint32 *)row;
        for (int j = 0; j < buffer->width; j ++) {
            // pixel in memory: 00 00 00 00
            uint8 blue = i + xOffset;
            uint8 green = j + yOffset;
            // pixel (32-bits): xx RR GG BB
            *pixel ++ = ((green << 8) | blue);
        }
        row += buffer->pitch;
    }
}

void GameUpdateAndRender(game_offscreen_buffer *buffer, int blueOffset, int greenOffset,
                         game_sound_output_buffer *soundBuffer, int toneHz) {
    GameOutputSound(soundBuffer, toneHz);
    RenderGradient(buffer, blueOffset, greenOffset);
}