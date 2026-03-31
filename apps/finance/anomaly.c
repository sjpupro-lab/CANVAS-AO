#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

typedef struct {
    int   index;
    float score;
    int   is_anomaly;
} AnomalyResult;

/* Compute mean for normalization */
static float compute_mean(const V6F *data, int count, int field) {
    float sum = 0.0f;
    for (int i = 0; i < count; i++) sum += data[i].v[field];
    return count > 0 ? sum / count : 0.0f;
}

static float compute_std(const V6F *data, int count, int field, float mean) {
    float var = 0.0f;
    for (int i = 0; i < count; i++) {
        float d = data[i].v[field] - mean;
        var += d * d;
    }
    return count > 0 ? sqrtf(var / count) : 1.0f;
}

float anomaly_score(const V6F *vec, float mean_price, float std_price) {
    if (std_price < 1e-6f) return 0.0f;
    float z = fabsf(vec->v[0] - mean_price) / std_price;
    /* Sigmoid transform: maps z-score to [0,1] */
    return 1.0f / (1.0f + expf(-0.5f * (z - 3.0f)));
}

int anomaly_detect(const V6F *data, int count, AnomalyResult *results) {
    if (!data || !results || count <= 0) return 0;

    EmotionVector market_sentiment;
    emotion_init(&market_sentiment);
    constellation_build(2048, 2048, 16);

    float mean = compute_mean(data, count, 0);
    float std  = compute_std(data, count, 0, mean);
    if (std < 1e-6f) std = 1.0f;

    int anomaly_count = 0;
    for (int i = 0; i < count; i++) {
        float score = anomaly_score(&data[i], mean, std);
        results[i].index      = i;
        results[i].score      = score;
        results[i].is_anomaly = (score > 0.5f) ? 1 : 0;

        /* Update market sentiment based on anomaly */
        if (results[i].is_anomaly) {
            emotion_update(&market_sentiment, EMOTION_FEAR, score * 0.3f);
            emotion_update(&market_sentiment, EMOTION_SURPRISE, score * 0.2f);
            anomaly_count++;
        } else {
            emotion_update(&market_sentiment, EMOTION_TRUST, 0.05f);
        }

        /* Update constellation energy */
        constellation_update(2048 + i % 32, 2048 + i / 32, score * 0.1f);
    }
    return anomaly_count;
}

void anomaly_print_report(const AnomalyResult *results, int count) {
    printf("=== Anomaly Detection Report ===\n");
    int total_anomalies = 0;
    for (int i = 0; i < count; i++) {
        if (results[i].is_anomaly) {
            printf("  [%3d] ANOMALY  score=%.4f\n", results[i].index, results[i].score);
            total_anomalies++;
        }
    }
    printf("Total anomalies: %d / %d\n", total_anomalies, count);
}
