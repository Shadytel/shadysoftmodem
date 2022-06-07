#include <stdio.h>
#include "resample.h"

int main(int argc, char *argv[]) {
    ResamplerState state;
    resamp_8khz_9k6hz_init(&state);
    int16_t inBuf[256];
    int16_t outBuf[512];

    while (!feof(stdin)) {
        size_t samplesRead = fread(&inBuf, sizeof(inBuf[0]), 256, stdin);
        size_t remainingSamples = resample(&state, inBuf, samplesRead, outBuf, 512);
        fwrite(outBuf, sizeof(int16_t), 512 - remainingSamples, stdout);
    }

    return 0;
}