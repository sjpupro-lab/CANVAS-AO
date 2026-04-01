/*
 * anomaly.c — Anomaly detection, integer-only (DK-2)
 *
 * Z-score computed in fixed-point ×256.
 * Anomaly score: uint8_t 0-255 (sigmoid approximation via lookup).
 */

#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int     index;
    uint8_t score;    /* 0-255 anomaly score */
    int     is_anomaly;
} AnomalyResult;

/* Integer sqrt via Newton's method */
static uint32_t isqrt(uint32_t n) {
    if (n == 0) return 0;
    uint32_t x = n, y = (x + 1) >> 1;
    while (y < x) { x = y; y = (x + n / x) >> 1; }
    return x;
}

/* Compute mean of a V6F field */
static int32_t compute_mean(const V6F *data, int count, int field) {
    if (count <= 0) return 0;
    int64_t sum = 0;
    for (int i = 0; i < count; i++) sum += data[i].v[field];
    return (int32_t)(sum / count);
}

/* Compute std deviation ×256 (fixed-point) */
static uint32_t compute_std_fp(const V6F *data, int count, int field, int32_t mean) {
    if (count <= 0) return 256;  /* default 1.0 in ×256 */
    int64_t var = 0;
    for (int i = 0; i < count; i++) {
        int64_t d = data[i].v[field] - mean;
        var += d * d;
    }
    var /= count;
    /* std = sqrt(var), return std × 256 */
    uint32_t std_int = isqrt((uint32_t)(var > 0xFFFFFFFF ? 0xFFFFFFFF : var));
    return std_int > 0 ? std_int * 256 : 256;
}

/*
 * Anomaly score: integer sigmoid approximation.
 * z = |price - mean| * 256 / std  (×256 fixed-point z-score)
 * Score: piecewise linear sigmoid around z=768 (z-score=3.0):
 *   z < 512 → 0, z 512-1024 → linear 0-255, z > 1024 → 255
 */
static uint8_t anomaly_score(const V6F *vec, int32_t mean_price, uint32_t std_fp) {
    if (std_fp == 0) return 0;
    int32_t diff = vec->v[0] - mean_price;
    if (diff < 0) diff = -diff;
    uint32_t z = ((uint32_t)diff * 256) / (std_fp / 256);  /* z in ×256 */

    if (z < 512) return 0;       /* z-score < 2.0 → no anomaly */
    if (z > 1024) return 255;    /* z-score > 4.0 → definite anomaly */
    return (uint8_t)((z - 512) * 255 / 512);
}

int anomaly_detect(const V6F *data, int count, AnomalyResult *results) {
    if (!data || !results || count <= 0) return 0;

    EmotionVector market_sentiment;
    emotion_init(&market_sentiment);
    constellation_build(2048, 2048, 16);

    int32_t  mean   = compute_mean(data, count, 0);
    uint32_t std_fp = compute_std_fp(data, count, 0, mean);

    int anomaly_count = 0;
    for (int i = 0; i < count; i++) {
        uint8_t score = anomaly_score(&data[i], mean, std_fp);
        results[i].index      = i;
        results[i].score      = score;
        results[i].is_anomaly = (score > 128) ? 1 : 0;

        /* Update market sentiment based on anomaly */
        if (results[i].is_anomaly) {
            /* fear intensity ≈ score * 0.3 → score * 77 / 256 */
            uint8_t fear_i = (uint8_t)(((uint16_t)score * 77 + 128) >> 8);
            uint8_t surp_i = (uint8_t)(((uint16_t)score * 51 + 128) >> 8);
            emotion_update(&market_sentiment, EMOTION_FEAR, fear_i);
            emotion_update(&market_sentiment, EMOTION_SURPRISE, surp_i);
            anomaly_count++;
        } else {
            emotion_update(&market_sentiment, EMOTION_TRUST, 13); /* ~0.05 */
        }

        /* Update constellation energy: delta ≈ score / 40 */
        int16_t delta = (int16_t)(score / 40);
        constellation_update(2048 + i % 32, 2048 + i / 32, delta);
    }
    return anomaly_count;
}

void anomaly_print_report(const AnomalyResult *results, int count) {
    printf("=== Anomaly Detection Report ===\n");
    int total_anomalies = 0;
    for (int i = 0; i < count; i++) {
        if (results[i].is_anomaly) {
            printf("  [%3d] ANOMALY  score=%d/255\n", results[i].index, results[i].score);
            total_anomalies++;
        }
    }
    printf("Total anomalies: %d / %d\n", total_anomalies, count);
}
