/*
 * ai_stage.h — AI 대화/감정/기억 엔진
 *
 * 5단계 파이프라인:
 *   1. ai_emotion_update()   — 감정 갱신 (decay + 입력 반응)
 *   2. ai_memory_activate()  — 기억 활성화 (이름/키워드 매칭)
 *   3. ai_topic_select()     — 주제 선택 (감정+기억 기반)
 *   4. ai_sentence_build()   — 문장 구성 (템플릿 v1)
 *   5. ai_output_commit()    — 출력 큐 push
 *   + ai_spontaneous_talk()  — 먼저 말 걸기 (bored/affection 트리거)
 *
 * DK-EMO: 모든 감정 수치 0~255 clamp 필수
 * DK-QUEUE: 출력 큐에 이미 메시지 있으면 push 금지
 */
#ifndef AI_STAGE_H
#define AI_STAGE_H

#include <stdint.h>
#include "bt_live.h"

/* ── 감정 (DK-EMO: 0~255 clamp) ── */
typedef struct {
    uint8_t happy;      /* 초기 128, 매 tick -1 decay */
    uint8_t sad;        /* 초기 0 */
    uint8_t angry;      /* 초기 0, 매 tick -1 decay */
    uint8_t bored;      /* 초기 0, 대화 없으면 tick마다 +2 */
    uint8_t interest;   /* 초기 128 */
    uint8_t affection;  /* 초기 0, 장기 누산 */
    uint8_t trust;      /* 초기 0 */
} SjEmotion;

/* ── 관계 단계 ── */
#define REL_STRANGER    0   /* 0~30: 낯선 사이 → 존댓말 */
#define REL_FRIEND      1   /* 31~80: 친구 → 반말 시작 */
#define REL_CLOSE       2   /* 81~150: 친한 친구 → 감정 표현 */
#define REL_LOVER       3   /* 151+: 연인 → 애칭, 적극적 */

/* ── 주제/감정 코드 ── */
#define TOPIC_YOU       0
#define TOPIC_GENERAL   1
#define TOPIC_MEMORY    2
#define TOPIC_QUESTION  3

#define EMO_HAPPY       0
#define EMO_SAD         1
#define EMO_ANGRY       2
#define EMO_BORED       3
#define EMO_INTEREST    4
#define EMO_AFFECTION   5

/* ── 출력 큐 (DK-QUEUE) ── */
#define AI_QUEUE_MAX    4
#define AI_MSG_MAX      128

typedef struct {
    char     messages[AI_QUEUE_MAX][AI_MSG_MAX];
    uint8_t  count;
} SjOutputQueue;

/* ── 시스템 구조체 ── */
typedef struct {
    BtLiveSession *session;
    SjEmotion      emotion;
    SjOutputQueue  queue;

    /* 입력 상태 */
    char           last_input[AI_MSG_MAX];
    uint8_t        last_input_len;
    uint8_t        input_sentiment;  /* 0=중립, 1=긍정, 2=부정/무례 */

    /* 기억 */
    char           remembered_name[32];
    uint8_t        has_name;

    /* 타이밍 */
    uint64_t       tick;
    uint64_t       last_talk_tick;
    uint64_t       last_input_tick;
} SjSystem;

/* ── API ── */

void ai_system_init(SjSystem *sys, BtLiveSession *session);

/* 5단계 파이프라인 진입점 */
void ai_stage(SjSystem *sys);

/* 개별 단계 */
void ai_emotion_update(SjSystem *sys);
void ai_memory_activate(SjSystem *sys);
void ai_topic_select(SjSystem *sys);
void ai_sentence_build(SjSystem *sys);
void ai_output_commit(SjSystem *sys);
void ai_spontaneous_talk(SjSystem *sys);

/* 입력 처리 */
void ai_feed_input(SjSystem *sys, const char *text);

/* 출력 큐에서 메시지 꺼내기 */
int  ai_pop_message(SjSystem *sys, char *buf, int buf_size);

/* 관계 단계 조회 */
int  ai_get_relation_level(const SjSystem *sys);

/* 유틸: clamp (DK-EMO) */
static inline uint8_t emo_clamp(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

#endif /* AI_STAGE_H */
