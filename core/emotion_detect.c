/*
 * emotion_detect.c — Byte-pattern emotion detection, integer-only (DK-2)
 *
 * 7개 감정(Plutchik)별 독립 n-gram 바이트 해시 테이블 유지.
 * 훈련: emotion_detect_train(text, len, EMOTION_JOY) → JOY 테이블에 바이트 패턴 기록
 * 추론: emotion_detect_infer(text, len) → 7개 테이블 각각에서 점수 합산 → 최고 점수 감정 반환
 *
 * 한글 UTF-8: 3바이트/글자이므로 order 3이면 1글자, order 6이면 2글자 패턴.
 * order 1-4로 충분한 바이트 패턴 포착 가능.
 */

#include "canvasos.h"
#include "cell.h"
#include <string.h>

#define ED_HASH_SIZE   16384
#define ED_MAX_ORDER   4
#define ED_NUM_EMOTIONS 7

/* 감정별 n-gram 테이블: pred[emotion][order][hash] */
static uint8_t  ed_pred[ED_NUM_EMOTIONS][ED_MAX_ORDER][ED_HASH_SIZE];
static uint16_t ed_conf[ED_NUM_EMOTIONS][ED_MAX_ORDER][ED_HASH_SIZE];
static int      ed_train_count[ED_NUM_EMOTIONS];
static int      ed_ready = 0;

static uint32_t ed_fnv1a(const uint8_t *data, int len) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h & (ED_HASH_SIZE - 1);
}

void emotion_detect_init(void) {
    memset(ed_pred, 0, sizeof(ed_pred));
    memset(ed_conf, 0, sizeof(ed_conf));
    memset(ed_train_count, 0, sizeof(ed_train_count));
    ed_ready = 1;
}

void emotion_detect_train(const uint8_t *text, int text_len, EmotionIndex emotion) {
    if (!ed_ready) emotion_detect_init();
    if (!text || text_len <= 0 || emotion < 0 || emotion >= ED_NUM_EMOTIONS) return;

    int eidx = (int)emotion;

    for (int i = 0; i < text_len; i++) {
        for (int order = 1; order <= ED_MAX_ORDER; order++) {
            if (i < order) continue;
            uint32_t h = ed_fnv1a(text + i - order, order);
            if (ed_pred[eidx][order - 1][h] == text[i]) {
                if (ed_conf[eidx][order - 1][h] < 65535)
                    ed_conf[eidx][order - 1][h]++;
            } else if (ed_conf[eidx][order - 1][h] == 0) {
                ed_pred[eidx][order - 1][h] = text[i];
                ed_conf[eidx][order - 1][h] = 1;
            }
        }
    }
    ed_train_count[eidx] += text_len;
}

/*
 * 추론: 입력 텍스트의 바이트 패턴이 어떤 감정 테이블에 가장 잘 매칭되는지 계산.
 *
 * 각 감정별 점수 = Σ(각 위치에서, 예측이 실제 바이트와 일치하면 order × conf)
 * 최고 점수 감정 반환. 훈련 데이터 없으면 EMOTION_JOY 기본값.
 */
EmotionIndex emotion_detect_infer(const uint8_t *text, int text_len) {
    if (!ed_ready) emotion_detect_init();
    if (!text || text_len <= 0) return EMOTION_JOY;

    uint32_t scores[ED_NUM_EMOTIONS];
    memset(scores, 0, sizeof(scores));

    for (int i = 0; i < text_len; i++) {
        for (int eidx = 0; eidx < ED_NUM_EMOTIONS; eidx++) {
            for (int order = 1; order <= ED_MAX_ORDER; order++) {
                if (i < order) continue;
                uint32_t h = ed_fnv1a(text + i - order, order);
                if (ed_pred[eidx][order - 1][h] == text[i]) {
                    /* 매칭! order × confidence 점수 가산 */
                    uint32_t conf = ed_conf[eidx][order - 1][h];
                    if (conf > 255) conf = 255; /* 점수 폭발 방지 */
                    scores[eidx] += (uint32_t)order * conf;
                }
            }
        }
    }

    /* 최고 점수 감정 선택 */
    EmotionIndex best = EMOTION_JOY;
    uint32_t best_score = scores[0];
    for (int i = 1; i < ED_NUM_EMOTIONS; i++) {
        if (scores[i] > best_score) {
            best_score = scores[i];
            best = (EmotionIndex)i;
        }
    }
    return best;
}

/*
 * 전체 점수 조회: 7개 감정별 점수를 0-255로 정규화하여 반환.
 */
void emotion_detect_scores(const uint8_t *text, int text_len, uint8_t *out_scores) {
    if (!ed_ready) emotion_detect_init();
    if (!out_scores) return;
    memset(out_scores, 0, ED_NUM_EMOTIONS);
    if (!text || text_len <= 0) return;

    uint32_t scores[ED_NUM_EMOTIONS];
    memset(scores, 0, sizeof(scores));

    for (int i = 0; i < text_len; i++) {
        for (int eidx = 0; eidx < ED_NUM_EMOTIONS; eidx++) {
            for (int order = 1; order <= ED_MAX_ORDER; order++) {
                if (i < order) continue;
                uint32_t h = ed_fnv1a(text + i - order, order);
                if (ed_pred[eidx][order - 1][h] == text[i]) {
                    uint32_t conf = ed_conf[eidx][order - 1][h];
                    if (conf > 255) conf = 255;
                    scores[eidx] += (uint32_t)order * conf;
                }
            }
        }
    }

    /* 최대값 기준 0-255 정규화 */
    uint32_t max_score = 0;
    for (int i = 0; i < ED_NUM_EMOTIONS; i++) {
        if (scores[i] > max_score) max_score = scores[i];
    }
    if (max_score == 0) return;
    for (int i = 0; i < ED_NUM_EMOTIONS; i++) {
        out_scores[i] = (uint8_t)((scores[i] * 255) / max_score);
    }
}

int emotion_detect_trained(EmotionIndex emotion) {
    if (!ed_ready || emotion < 0 || emotion >= ED_NUM_EMOTIONS) return 0;
    return ed_train_count[(int)emotion];
}
