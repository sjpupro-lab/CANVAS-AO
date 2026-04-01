/*
 * emotion.c — 7D emotion vector, integer-only (DK-2)
 *
 * All values are uint8_t 0-255 (mapped from original 0.0-1.0 float).
 * Decay uses integer EMA: val = (val * 250 + 128) >> 8  (~0.977x ≈ 0.98f)
 * Blend uses: out = (a * (255-ratio) + b * ratio + 128) >> 8
 */

#include "canvasos.h"
#include "cell.h"
#include <string.h>

void emotion_init(EmotionVector *ev) {
    memset(ev, 0, sizeof(*ev));
}

void emotion_update(EmotionVector *ev, EmotionIndex stimulus, uint8_t intensity) {
    uint8_t *fields[7] = {
        &ev->joy, &ev->trust, &ev->fear, &ev->surprise,
        &ev->sadness, &ev->disgust, &ev->anger
    };
    if (stimulus < 0 || stimulus > 6) return;

    /* Add intensity with saturation */
    uint16_t sum = (uint16_t)*fields[stimulus] + intensity;
    if (sum > 255) sum = 255;
    *fields[stimulus] = (uint8_t)sum;

    /* Decay others: val = (val * 250 + 128) >> 8  ≈ 0.977x */
    for (int i = 0; i < 7; i++) {
        if (i != (int)stimulus) {
            *fields[i] = (uint8_t)(((uint16_t)*fields[i] * 250 + 128) >> 8);
        }
    }
}

EmotionVector emotion_blend(EmotionVector a, EmotionVector b, uint8_t ratio) {
    EmotionVector out;
    uint8_t s = 255 - ratio;
    /* out = (a * s + b * ratio + 128) >> 8 */
    out.joy      = (uint8_t)(((uint16_t)a.joy      * s + (uint16_t)b.joy      * ratio + 128) >> 8);
    out.trust    = (uint8_t)(((uint16_t)a.trust    * s + (uint16_t)b.trust    * ratio + 128) >> 8);
    out.fear     = (uint8_t)(((uint16_t)a.fear     * s + (uint16_t)b.fear     * ratio + 128) >> 8);
    out.surprise = (uint8_t)(((uint16_t)a.surprise * s + (uint16_t)b.surprise * ratio + 128) >> 8);
    out.sadness  = (uint8_t)(((uint16_t)a.sadness  * s + (uint16_t)b.sadness  * ratio + 128) >> 8);
    out.disgust  = (uint8_t)(((uint16_t)a.disgust  * s + (uint16_t)b.disgust  * ratio + 128) >> 8);
    out.anger    = (uint8_t)(((uint16_t)a.anger    * s + (uint16_t)b.anger    * ratio + 128) >> 8);
    return out;
}

EmotionIndex emotion_dominant(EmotionVector *ev) {
    uint8_t vals[7] = {
        ev->joy, ev->trust, ev->fear, ev->surprise,
        ev->sadness, ev->disgust, ev->anger
    };
    int     best = 0;
    uint8_t bval = vals[0];
    for (int i = 1; i < 7; i++) {
        if (vals[i] > bval) { bval = vals[i]; best = i; }
    }
    return (EmotionIndex)best;
}

uint8_t emotion_to_energy(EmotionVector *ev) {
    uint16_t sum = (uint16_t)ev->joy + ev->trust + ev->fear + ev->surprise +
                   ev->sadness + ev->disgust + ev->anger;
    /* Average of 7 fields, result 0-255 */
    return (uint8_t)((sum + 3) / 7);  /* +3 for rounding */
}
