#include <stdio.h>
#include "resample.h"

int main(int argc, char *argv[]) {
    resamp_8khz_9k6hz_init();
    int16_t inBuf[512];
    int16_t outBuf[512];

    while (!feof(stdin)) {
        size_t samplesRead = fread(&inBuf, sizeof(inBuf[0]), 512, stdin);
        size_t remainingSamples = resamp_8khz_9k6hz(inBuf, samplesRead, outBuf, 512);
        fwrite(outBuf, sizeof(int16_t), 512 - remainingSamples, stdout);
    }

    return 0;
}