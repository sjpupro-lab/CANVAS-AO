/*
 * pattern.c — 6-layer spatial pattern recognition, integer-only (DK-2)
 *
 * All scores are uint8_t 0-255 (mapped from original 0.0-1.0 float).
 * Distance uses integer squared distance instead of sqrtf.
 */

#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_TRAINED 128

static PatternEntry trained[MAX_TRAINED];
static int          trained_count = 0;

/* score = energy (already 0-255) */
static uint8_t layer_score_raw(int x, int y) {
    Cell c = canvas_get(x, y);
    return c.energy;
}

/* Average absolute energy difference with neighbors, 0-255 */
static uint8_t layer_score_edge(int x, int y, int radius) {
    Cell center = canvas_get(x, y);
    uint32_t diff = 0;
    int      cnt  = 0;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx == 0 && dy == 0) continue;
            Cell n = canvas_get(x + dx, y + dy);
            int d = (int)center.energy - (int)n.energy;
            diff += (uint32_t)(d < 0 ? -d : d);
            cnt++;
        }
    }
    if (cnt == 0) return 0;
    return (uint8_t)(diff / (uint32_t)cnt);
}

/* Fraction of active cells in area, scaled to 0-255 */
static uint8_t layer_score_shape(int x, int y, int radius) {
    int active = 0, total = 0;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            Cell c = canvas_get(x + dx, y + dy);
            if (c.state == CELL_ACTIVE) active++;
            total++;
        }
    }
    if (total == 0) return 0;
    return (uint8_t)((active * 255) / total);
}

/* Average cell ID normalized to 0-255 */
static uint8_t layer_score_object(int x, int y, int radius) {
    uint32_t sum_id = 0;
    int      cnt    = 0;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            Cell c = canvas_get(x + dx, y + dy);
            sum_id += c.id;
            cnt++;
        }
    }
    if (cnt == 0) return 0;
    uint32_t avg = sum_id / (uint32_t)cnt;
    /* Normalize: avg/65535 * 255, capped at 255 */
    uint32_t norm = (avg * 255) / 65535;
    return (uint8_t)(norm > 255 ? 255 : norm);
}

/* context = (shape*2_score + object_score) / 2 */
static uint8_t layer_score_context(int x, int y, int radius) {
    int r2 = radius * 2 > 10 ? 10 : radius * 2;
    uint16_t s = layer_score_shape(x, y, r2);
    uint16_t o = layer_score_object(x, y, radius);
    return (uint8_t)((s + o + 1) >> 1);
}

/* abstract = (raw + edge + shape) / 3 */
static uint8_t layer_score_abstract(int x, int y, int radius) {
    uint16_t r = layer_score_raw(x, y);
    uint16_t e = layer_score_edge(x, y, radius);
    uint16_t s = layer_score_shape(x, y, radius);
    return (uint8_t)((r + e + s + 1) / 3);
}

void pattern_layer_raw(int x, int y) {
    (void)layer_score_raw(x, y);
}

void pattern_layer_edge(int x, int y, int radius) {
    (void)layer_score_edge(x, y, radius);
}

void pattern_layer_shape(int x, int y, int radius) {
    (void)layer_score_shape(x, y, radius);
}

void pattern_layer_object(int x, int y, int radius) {
    (void)layer_score_object(x, y, radius);
}

void pattern_layer_context(int x, int y, int radius) {
    (void)layer_score_context(x, y, radius);
}

void pattern_layer_abstract(int x, int y, int radius) {
    (void)layer_score_abstract(x, y, radius);
}

PatternResult pattern_recognize(int x, int y, int radius) {
    PatternResult res;
    uint8_t scores[6];
    scores[0] = layer_score_raw(x, y);
    scores[1] = layer_score_edge(x, y, radius);
    scores[2] = layer_score_shape(x, y, radius);
    scores[3] = layer_score_object(x, y, radius);
    scores[4] = layer_score_context(x, y, radius);
    scores[5] = layer_score_abstract(x, y, radius);

    int     best_layer = 0;
    uint8_t best_score = scores[0];
    for (int i = 1; i < 6; i++) {
        if (scores[i] > best_score) {
            best_score = scores[i];
            best_layer = i;
        }
    }

    res.layer      = (PatternLayer)best_layer;
    res.confidence = best_score;
    res.matched_id = 0;

    /* Check trained patterns — use squared distance (no sqrtf) */
    for (int i = 0; i < trained_count; i++) {
        if (trained[i].layer == (PatternLayer)best_layer) {
            int dx = trained[i].x - x;
            int dy = trained[i].y - y;
            int dist_sq = dx * dx + dy * dy;
            int thresh  = (radius + 1) * (radius + 1);
            if (dist_sq < thresh) {
                res.matched_id = trained[i].id;
                break;
            }
        }
    }
    return res;
}

void pattern_train(int x, int y, PatternLayer layer, uint16_t id) {
    if (trained_count >= MAX_TRAINED) return;
    trained[trained_count].x          = x;
    trained[trained_count].y          = y;
    trained[trained_count].layer      = layer;
    trained[trained_count].id         = id;
    trained[trained_count].confidence = 255;
    trained_count++;
}

/* ===================================================================
 * N-gram Language Pattern Engine (6 layers: byte → sentence)
 *
 * Layers map to n-gram orders 1-6:
 *   LANG_BYTE (order 1)     — single byte frequency
 *   LANG_MORPHEME (order 2) — byte pairs
 *   LANG_WORD (order 3)     — short contexts
 *   LANG_PHRASE (order 4)   — medium contexts
 *   LANG_CLAUSE (order 5)   — long contexts
 *   LANG_SENTENCE (order 6) — full sentence patterns
 *
 * All integer-only (DK-2). FNV-1a hash, same as compress.c engine.
 * =================================================================== */

#define LANG_HASH_SIZE  65536
#define LANG_MAX_ORDER  6

static uint8_t  lang_pred[LANG_MAX_ORDER][LANG_HASH_SIZE];
static uint16_t lang_conf[LANG_MAX_ORDER][LANG_HASH_SIZE];
static int      lang_trained_count = 0;

static uint32_t lang_fnv1a(const uint8_t *data, int len) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h & (LANG_HASH_SIZE - 1);
}

void pattern_lang_init(void) {
    memset(lang_pred, 0, sizeof(lang_pred));
    memset(lang_conf, 0, sizeof(lang_conf));
    lang_trained_count = 0;
}

void pattern_lang_train(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;
    for (size_t i = 0; i < len; i++) {
        for (int order = 1; order <= LANG_MAX_ORDER; order++) {
            if (i < (size_t)order) continue;
            uint32_t h = lang_fnv1a(data + i - order, order);
            if (lang_pred[order - 1][h] == data[i]) {
                if (lang_conf[order - 1][h] < 65535)
                    lang_conf[order - 1][h]++;
            } else {
                lang_pred[order - 1][h] = data[i];
                lang_conf[order - 1][h] = 1;
            }
        }
    }
    lang_trained_count += (int)len;
}

LangPatternResult pattern_lang_recognize(const uint8_t *ctx, int ctx_len) {
    LangPatternResult res = {0};
    if (!ctx || ctx_len <= 0) return res;

    uint8_t  best_pred  = 0;
    uint32_t best_weight = 0;
    int      best_layer = 0;

    for (int order = 1; order <= LANG_MAX_ORDER; order++) {
        if (ctx_len < order) break;
        uint32_t h = lang_fnv1a(ctx + ctx_len - order, order);
        uint16_t conf = lang_conf[order - 1][h];
        /* Weight: order × confidence (higher order = higher preference) */
        uint32_t w = (uint32_t)order * conf;
        if (w > best_weight) {
            best_weight = w;
            best_pred   = lang_pred[order - 1][h];
            best_layer  = order - 1;
        }
    }

    res.layer      = (LangLayer)best_layer;
    res.prediction = best_pred;
    /* Confidence: clamp best_weight to 0-255 */
    res.confidence = (uint8_t)(best_weight > 255 ? 255 : best_weight);
    res.order      = best_layer + 1;
    return res;
}

uint8_t pattern_lang_predict(const uint8_t *ctx, int ctx_len) {
    LangPatternResult r = pattern_lang_recognize(ctx, ctx_len);
    return r.prediction;
}

uint8_t pattern_lang_confidence(const uint8_t *ctx, int ctx_len) {
    LangPatternResult r = pattern_lang_recognize(ctx, ctx_len);
    return r.confidence;
}

int pattern_lang_trained_count(void) {
    return lang_trained_count;
}
