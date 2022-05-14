#include <stdint.h>

typedef struct {
    int32_t * taps;
    int16_t * history;
    size_t historyIdx;
    size_t samplesPerPhase;
    size_t phase;
    size_t phases;
    size_t phaseIncrement;
} ResamplerState;

void resamp_9k6hz_8khz_init(ResamplerState * state);
void resamp_9k6hz_16khz_init(ResamplerState * state);
void resamp_8khz_9k6hz_init(ResamplerState * state);
void resamp_16khz_9k6hz_init(ResamplerState * state);

size_t resample(ResamplerState * state, int16_t * in, size_t inCount,
    int16_t * out, size_t maxOut);