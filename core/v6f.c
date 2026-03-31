#include "canvasos.h"
#include "cell.h"
#include <math.h>
#include <string.h>

V6F v6f_encode(float price, float open, float high, float low,
               float volume, float ts) {
    V6F f;
    f.v[0] = price;
    f.v[1] = open;
    f.v[2] = high;
    f.v[3] = low;
    f.v[4] = volume;
    f.v[5] = ts;
    return f;
}

void v6f_decode(const V6F *f, float *price, float *open, float *high,
                float *low, float *volume, float *ts) {
    if (price)  *price  = f->v[0];
    if (open)   *open   = f->v[1];
    if (high)   *high   = f->v[2];
    if (low)    *low    = f->v[3];
    if (volume) *volume = f->v[4];
    if (ts)     *ts     = f->v[5];
}

float v6f_distance(const V6F *a, const V6F *b) {
    float sum = 0.0f;
    for (int i = 0; i < 6; i++) {
        float d = a->v[i] - b->v[i];
        sum += d * d;
    }
    return sqrtf(sum);
}

float v6f_similarity(const V6F *a, const V6F *b) {
    float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
    for (int i = 0; i < 6; i++) {
        dot   += a->v[i] * b->v[i];
        mag_a += a->v[i] * a->v[i];
        mag_b += b->v[i] * b->v[i];
    }
    float denom = sqrtf(mag_a) * sqrtf(mag_b);
    if (denom < 1e-9f) return 0.0f;
    return dot / denom;
}
