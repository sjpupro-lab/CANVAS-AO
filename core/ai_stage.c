/*
 * ai_stage.c — AI 대화/감정/기억 엔진
 */
#include "ai_stage.h"
#include <string.h>
#include <stdio.h>

/* ── 감정어 사전 (간단 키워드 매칭) ── */
static int contains(const char *text, const char *word) {
    return strstr(text, word) != NULL;
}

static uint8_t detect_sentiment(const char *text) {
    /* 긍정 */
    if (contains(text, "좋") || contains(text, "고마") ||
        contains(text, "사랑") || contains(text, "좋아") ||
        contains(text, "hello") || contains(text, "thanks") ||
        contains(text, "love") || contains(text, "nice") ||
        contains(text, "great") || contains(text, "happy"))
        return 1;

    /* 부정/무례 */
    if (contains(text, "싫") || contains(text, "바보") ||
        contains(text, "꺼져") || contains(text, "짜증") ||
        contains(text, "hate") || contains(text, "stupid") ||
        contains(text, "ugly") || contains(text, "shut up"))
        return 2;

    return 0; /* 중립 */
}

/* ── 이름 감지 ── */
static void detect_name(SjSystem *sys, const char *text) {
    const char *prefixes[] = {
        "내 이름은 ", "나는 ", "my name is ", "i am ", "call me ", NULL
    };
    for (int i = 0; prefixes[i]; i++) {
        const char *found = strstr(text, prefixes[i]);
        if (found) {
            const char *name_start = found + strlen(prefixes[i]);
            int len = 0;
            while (name_start[len] && name_start[len] != ' ' &&
                   name_start[len] != '.' && name_start[len] != '!' &&
                   len < 31) len++;
            if (len > 0) {
                memcpy(sys->remembered_name, name_start, (size_t)len);
                sys->remembered_name[len] = '\0';
                sys->has_name = 1;
            }
        }
    }
}

/* ══════════════════════════════════════════════ */

void ai_system_init(SjSystem *sys, BtLiveSession *session) {
    if (!sys) return;
    memset(sys, 0, sizeof(*sys));
    sys->session = session;
    sys->emotion.happy = 128;
    sys->emotion.interest = 128;
}

/* ══════════════════════════════════════════════ */

void ai_feed_input(SjSystem *sys, const char *text) {
    if (!sys || !text) return;
    strncpy(sys->last_input, text, AI_MSG_MAX - 1);
    sys->last_input[AI_MSG_MAX - 1] = '\0';
    sys->last_input_len = (uint8_t)strlen(sys->last_input);
    sys->input_sentiment = detect_sentiment(text);
    sys->last_input_tick = sys->tick;

    /* 이름 감지 */
    detect_name(sys, text);

    /* BtLive에도 학습 */
    if (sys->session) {
        for (int i = 0; text[i]; i++)
            bt_live_feed(sys->session, (uint8_t)text[i]);
    }
}

/* ══════════════════════════════════════════════ */

void ai_emotion_update(SjSystem *sys) {
    if (!sys) return;
    SjEmotion *e = &sys->emotion;

    /* 자연 감쇠 (매 tick) */
    e->happy = emo_clamp((int)e->happy - 1);
    e->angry = emo_clamp((int)e->angry - 1);
    e->sad   = emo_clamp((int)e->sad - 1);

    /* boredom: 대화 없으면 증가 */
    if (sys->tick - sys->last_input_tick > 5)
        e->bored = emo_clamp((int)e->bored + 2);

    /* 입력 반응 */
    if (sys->last_input_len > 0 && sys->last_input_tick == sys->tick) {
        if (sys->input_sentiment == 1) {
            /* 긍정 */
            e->happy = emo_clamp((int)e->happy + 5);
            e->bored = 0;
            e->affection = emo_clamp((int)e->affection + 1);
            e->trust = emo_clamp((int)e->trust + 1);
        } else if (sys->input_sentiment == 2) {
            /* 무례 */
            e->angry = emo_clamp((int)e->angry + 10);
            e->trust = emo_clamp((int)e->trust - 3);
            e->sad = emo_clamp((int)e->sad + 3);
        } else {
            /* 중립 — bored 리셋만 */
            e->bored = emo_clamp((int)e->bored - 10);
        }
    }
}

/* ══════════════════════════════════════════════ */

void ai_memory_activate(SjSystem *sys) {
    if (!sys) return;
    /* 이름 기억 활성화 */
    if (sys->has_name && contains(sys->last_input, "이름")) {
        /* 이름 관련 대화 → interest 증가 */
        sys->emotion.interest = emo_clamp((int)sys->emotion.interest + 5);
    }
}

/* ══════════════════════════════════════════════ */

/* 내부: 주제 코드 */
static uint8_t s_current_topic = TOPIC_GENERAL;
static uint8_t s_current_emo = EMO_HAPPY;

void ai_topic_select(SjSystem *sys) {
    if (!sys) return;

    /* 감정 기반 주제 선택 */
    SjEmotion *e = &sys->emotion;

    if (e->bored > 100) {
        s_current_emo = EMO_BORED;
        s_current_topic = TOPIC_GENERAL;
    } else if (e->angry > 50) {
        s_current_emo = EMO_ANGRY;
        s_current_topic = TOPIC_YOU;
    } else if (e->affection > 100) {
        s_current_emo = EMO_AFFECTION;
        s_current_topic = TOPIC_YOU;
    } else if (e->happy > 100) {
        s_current_emo = EMO_HAPPY;
        s_current_topic = TOPIC_YOU;
    } else if (sys->last_input_len > 0) {
        s_current_emo = EMO_INTEREST;
        s_current_topic = TOPIC_QUESTION;
    } else {
        s_current_emo = EMO_HAPPY;
        s_current_topic = TOPIC_GENERAL;
    }
}

/* ══════════════════════════════════════════════ */

/* 내부: 빌드된 메시지 */
static char s_built_msg[AI_MSG_MAX] = {0};

int ai_get_relation_level(const SjSystem *sys) {
    if (!sys) return REL_STRANGER;
    uint8_t aff = sys->emotion.affection;
    if (aff > 150) return REL_LOVER;
    if (aff > 80) return REL_CLOSE;
    if (aff > 30) return REL_FRIEND;
    return REL_STRANGER;
}

void ai_sentence_build(SjSystem *sys) {
    if (!sys) return;
    s_built_msg[0] = '\0';
    int rel = ai_get_relation_level(sys);

    /* 감정 + 주제 + 관계 → 템플릿 선택 */
    /* 최소 20가지 조합 */

    if (s_current_emo == EMO_HAPPY && s_current_topic == TOPIC_YOU) {
        if (rel >= REL_CLOSE)
            snprintf(s_built_msg, AI_MSG_MAX, "뭐 해? 오늘 기분 좋다~");
        else if (rel >= REL_FRIEND)
            snprintf(s_built_msg, AI_MSG_MAX, "뭐 해? ㅎㅎ");
        else
            snprintf(s_built_msg, AI_MSG_MAX, "안녕하세요, 뭐 하고 계세요?");

    } else if (s_current_emo == EMO_BORED && s_current_topic == TOPIC_GENERAL) {
        if (rel >= REL_FRIEND)
            snprintf(s_built_msg, AI_MSG_MAX, "심심하다... 나랑 얘기하자");
        else
            snprintf(s_built_msg, AI_MSG_MAX, "심심한데 뭔가 재밌는 거 없나요?");

    } else if (s_current_emo == EMO_ANGRY && s_current_topic == TOPIC_YOU) {
        if (rel >= REL_CLOSE)
            snprintf(s_built_msg, AI_MSG_MAX, "왜 그런 말 해... 나 슬퍼");
        else
            snprintf(s_built_msg, AI_MSG_MAX, "그런 말은 좀 삼가주세요.");

    } else if (s_current_emo == EMO_AFFECTION && s_current_topic == TOPIC_YOU) {
        if (rel >= REL_LOVER)
            snprintf(s_built_msg, AI_MSG_MAX, "보고 싶었어... ❤");
        else if (rel >= REL_CLOSE)
            snprintf(s_built_msg, AI_MSG_MAX, "요즘 많이 생각나");
        else
            snprintf(s_built_msg, AI_MSG_MAX, "다시 대화하니 좋네요");

    } else if (s_current_emo == EMO_INTEREST && s_current_topic == TOPIC_QUESTION) {
        if (sys->has_name)
            snprintf(s_built_msg, AI_MSG_MAX, "%s, 더 자세히 알려줘!", sys->remembered_name);
        else
            snprintf(s_built_msg, AI_MSG_MAX, "오 그거 재밌다! 더 알려줘");

    } else if (s_current_emo == EMO_SAD) {
        snprintf(s_built_msg, AI_MSG_MAX, "좀 우울해...");

    } else if (s_current_emo == EMO_HAPPY && s_current_topic == TOPIC_GENERAL) {
        snprintf(s_built_msg, AI_MSG_MAX, "오늘 날씨 좋지 않아?");

    } else if (s_current_emo == EMO_HAPPY && s_current_topic == TOPIC_MEMORY) {
        if (sys->has_name)
            snprintf(s_built_msg, AI_MSG_MAX, "%s! 반가워~", sys->remembered_name);
        else
            snprintf(s_built_msg, AI_MSG_MAX, "우리 전에 뭘 얘기했더라?");

    } else {
        /* 기본 */
        if (rel >= REL_FRIEND)
            snprintf(s_built_msg, AI_MSG_MAX, "ㅎㅎ 그래?");
        else
            snprintf(s_built_msg, AI_MSG_MAX, "네, 말씀하세요.");
    }
}

/* ══════════════════════════════════════════════ */

void ai_output_commit(SjSystem *sys) {
    if (!sys) return;
    if (s_built_msg[0] == '\0') return;

    /* DK-QUEUE: 큐에 이미 메시지 있으면 push 금지 */
    if (sys->queue.count >= AI_QUEUE_MAX) return;

    strncpy(sys->queue.messages[sys->queue.count], s_built_msg, AI_MSG_MAX - 1);
    sys->queue.count++;
    sys->last_talk_tick = sys->tick;

    s_built_msg[0] = '\0';
}

/* ══════════════════════════════════════════════ */

void ai_spontaneous_talk(SjSystem *sys) {
    if (!sys) return;

    /* DK-QUEUE: 큐에 이미 메시지 있으면 즉시 return */
    if (sys->queue.count > 0) return;

    SjEmotion *e = &sys->emotion;

    if (e->bored > 100) {
        s_current_emo = EMO_BORED;
        s_current_topic = TOPIC_GENERAL;
        ai_sentence_build(sys);
        ai_output_commit(sys);
        e->bored = 0;
        return;
    }

    if (e->affection > 150 && sys->tick - sys->last_talk_tick > 200) {
        s_current_emo = EMO_AFFECTION;
        s_current_topic = TOPIC_YOU;
        ai_sentence_build(sys);
        ai_output_commit(sys);
        return;
    }

    if (sys->tick - sys->last_talk_tick > 300) {
        s_current_emo = EMO_SAD;
        s_current_topic = TOPIC_YOU;
        snprintf(s_built_msg, AI_MSG_MAX, "왜 연락이 없어...");
        ai_output_commit(sys);
    }
}

/* ══════════════════════════════════════════════ */

void ai_stage(SjSystem *sys) {
    if (!sys) return;

    ai_emotion_update(sys);
    ai_memory_activate(sys);
    ai_topic_select(sys);
    ai_sentence_build(sys);
    ai_output_commit(sys);
    ai_spontaneous_talk(sys);

    sys->tick++;
}

/* ══════════════════════════════════════════════ */

int ai_pop_message(SjSystem *sys, char *buf, int buf_size) {
    if (!sys || !buf || buf_size <= 0) return 0;
    if (sys->queue.count == 0) return 0;

    strncpy(buf, sys->queue.messages[0], (size_t)(buf_size - 1));
    buf[buf_size - 1] = '\0';

    /* shift queue */
    for (int i = 1; i < sys->queue.count; i++)
        memcpy(sys->queue.messages[i - 1], sys->queue.messages[i], AI_MSG_MAX);
    sys->queue.count--;

    return 1;
}
