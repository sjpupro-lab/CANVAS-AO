/*
 * v6f.c — 6-element integer financial vector, integer-only (DK-2)
 *
 * All values stored as int32_t. Callers use fixed-point convention
 * (e.g., price in cents: $100.50 → 10050).
 *
 * Distance returns squared Euclidean (no sqrt).
 * Similarity returns integer cosine × 10000 (range -10000 to 10000).
 */

#include "canvasos.h"
#include "cell.h"
#include <string.h>

V6F v6f_encode(int32_t price, int32_t open, int32_t high, int32_t low,
               int32_t volume, int32_t ts) {
    V6F f;
    f.v[0] = price;
    f.v[1] = open;
    f.v[2] = high;
    f.v[3] = low;
    f.v[4] = volume;
    f.v[5] = ts;
    return f;
}

void v6f_decode(const V6F *f, int32_t *price, int32_t *open, int32_t *high,
                int32_t *low, int32_t *volume, int32_t *ts) {
    if (price)  *price  = f->v[0];
    if (open)   *open   = f->v[1];
    if (high)   *high   = f->v[2];
    if (low)    *low    = f->v[3];
    if (volume) *volume = f->v[4];
    if (ts)     *ts     = f->v[5];
}

/* Squared Euclidean distance (no sqrt needed) */
uint32_t v6f_distance_sq(const V6F *a, const V6F *b) {
    uint32_t sum = 0;
    for (int i = 0; i < 6; i++) {
        int32_t d = a->v[i] - b->v[i];
        sum += (uint32_t)(d * d);
    }
    return sum;
}

/*
 * Integer cosine similarity × 10000.
 * Returns range [-10000, 10000] where 10000 = identical direction.
 * Uses int64_t intermediates to avoid overflow.
 * Returns 0 for zero-magnitude vectors.
 */
int32_t v6f_similarity(const V6F *a, const V6F *b) {
    int64_t dot = 0, mag_a = 0, mag_b = 0;
    for (int i = 0; i < 6; i++) {
        dot   += (int64_t)a->v[i] * b->v[i];
        mag_a += (int64_t)a->v[i] * a->v[i];
        mag_b += (int64_t)b->v[i] * b->v[i];
    }
    if (mag_a == 0 || mag_b == 0) return 0;

    /* Integer sqrt approximation via Newton's method */
    /* We need sqrt(mag_a * mag_b). Use 64-bit product. */
    uint64_t product = (uint64_t)mag_a * (uint64_t)mag_b;

    /* Newton's method for isqrt(product) */
    if (product == 0) return 0;
    uint64_t x = product;
    uint64_t y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + product / x) >> 1;
    }
    /* x = isqrt(product) = sqrt(mag_a) * sqrt(mag_b) approximately */

    if (x == 0) return 0;
    /* cosine = dot / sqrt(mag_a * mag_b), scaled by 10000 */
    return (int32_t)((dot * 10000) / (int64_t)x);
}
