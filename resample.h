#include <stdint.h>

void resamp_8khz_9k6hz_init();
size_t resamp_8khz_9k6hz(int16_t * in, size_t inCount, int16_t * out, size_t maxOut);