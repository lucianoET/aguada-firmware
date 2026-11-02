#pragma once
#include <stdint.h>

// G.711 µ-law: 16 bit → 8 bit companding (metade da banda, qualidade de voz).

static inline uint8_t ulaw_encode(int16_t pcm) {
    const int BIAS = 0x84, CLIP = 32635;
    int sign = (pcm >> 8) & 0x80;
    int s = sign ? -(int)pcm : pcm;
    if (s > CLIP) s = CLIP;
    s += BIAS;
    int exp = 7;
    for (int mask = 0x4000; (s & mask) == 0 && exp > 0; mask >>= 1) exp--;
    int mant = (s >> (exp + 3)) & 0x0F;
    return (uint8_t)~(sign | (exp << 4) | mant);
}

static inline int16_t ulaw_decode(uint8_t u) {
    u = ~u;
    int sign = u & 0x80, exp = (u >> 4) & 0x07, mant = u & 0x0F;
    int s = ((mant << 3) + 0x84) << exp;
    s -= 0x84;
    return (int16_t)(sign ? -s : s);
}
