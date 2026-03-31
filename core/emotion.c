#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <math.h>

void emotion_init(EmotionVector *ev) {
    memset(ev, 0, sizeof(*ev));
}

void emotion_update(EmotionVector *ev, EmotionIndex stimulus, float intensity) {
    float *fields[7] = {
        &ev->joy, &ev->trust, &ev->fear, &ev->surprise,
        &ev->sadness, &ev->disgust, &ev->anger
    };
    if (stimulus < 0 || stimulus > 6) return;
    *fields[stimulus] += intensity;
    /* Clamp all to [0,1] and apply slight decay to others */
    for (int i = 0; i < 7; i++) {
        if (i != (int)stimulus) *fields[i] *= 0.98f;
        if (*fields[i] < 0.0f) *fields[i] = 0.0f;
        if (*fields[i] > 1.0f) *fields[i] = 1.0f;
    }
}

EmotionVector emotion_blend(EmotionVector a, EmotionVector b, float ratio) {
    EmotionVector out;
    float r = ratio < 0.0f ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);
    float s = 1.0f - r;
    out.joy      = a.joy      * s + b.joy      * r;
    out.trust    = a.trust    * s + b.trust    * r;
    out.fear     = a.fear     * s + b.fear     * r;
    out.surprise = a.surprise * s + b.surprise * r;
    out.sadness  = a.sadness  * s + b.sadness  * r;
    out.disgust  = a.disgust  * s + b.disgust  * r;
    out.anger    = a.anger    * s + b.anger    * r;
    return out;
}

EmotionIndex emotion_dominant(EmotionVector *ev) {
    float vals[7] = {
        ev->joy, ev->trust, ev->fear, ev->surprise,
        ev->sadness, ev->disgust, ev->anger
    };
    int   best = 0;
    float bval = vals[0];
    for (int i = 1; i < 7; i++) {
        if (vals[i] > bval) { bval = vals[i]; best = i; }
    }
    return (EmotionIndex)best;
}

float emotion_to_energy(EmotionVector *ev) {
    float sum = ev->joy + ev->trust + ev->fear + ev->surprise +
                ev->sadness + ev->disgust + ev->anger;
    return sum / 7.0f;
}
