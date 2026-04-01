/*
 * dating_engine.c — Dating AI response engine, integer-only (DK-2)
 *
 * Uses branch/multiverse for response diversity:
 *   1. Spawn 3 candidate universes, each with a different emotion variant
 *   2. Score each via constellation energy + affection alignment
 *   3. Collapse to best universe → select response
 *   4. Apply emotion→constellation feedback
 */

#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations from persona.c */
void           persona_load(const char *name);
void           persona_update_affection(uint8_t delta);
int            persona_get_response_style(void);
uint8_t        persona_get_affection(void);
const char    *persona_get_name(void);
const uint8_t *persona_get_vector(void);

static EmotionVector dating_emotion;
static int           dating_ready = 0;

/* Response bank indexed by dominant emotion × style */
static const char *responses[7][3] = {
    /* JOY */      {"정말 기뻐요!", "같이 있으면 행복해요~", "웃음이 나네요 😊"},
    /* TRUST */    {"믿어요, 당신을.", "솔직히 말해줘서 고마워요.", "신뢰가 쌓이는 것 같아요."},
    /* FEAR */     {"조금 긴장돼요...", "걱정되긴 하지만 괜찮아요.", "무서운 건 아니에요."},
    /* SURPRISE */ {"어머, 그래요?!", "예상 못 했어요!", "와, 놀랍네요!"},
    /* SADNESS */  {"조금 슬프네요...", "힘든 하루였어요.", "위로가 필요해요."},
    /* DISGUST */  {"음... 좀 별로네요.", "그건 좀 아닌 것 같아요.", "취향이 다르네요."},
    /* ANGER */    {"솔직히 좀 화가 나요.", "이해가 안 가요.", "왜 그러는지 모르겠어요."}
};

void dating_init(void) {
    canvas_init();
    branch_system_init();
    multiverse_init();
    persona_load("ELO");
    emotion_init(&dating_emotion);
    wh_init();
    lane_init();
    constellation_build(512, 512, 8);
    dating_ready = 1;
}

/*
 * Score a candidate response by how well its emotion aligns with
 * persona personality + current affection level.
 * Returns 0-255 score.
 */
static uint8_t score_candidate(EmotionIndex dom, int style) {
    const uint8_t *pvec = persona_get_vector();
    uint8_t aff = persona_get_affection();

    /* Base: persona alignment with this emotion */
    uint8_t persona_score = pvec[dom];

    /* Affection bonus: higher affection → prefer JOY/TRUST responses */
    uint8_t aff_bonus = 0;
    if (dom == EMOTION_JOY || dom == EMOTION_TRUST)
        aff_bonus = aff >> 2;  /* 0-63 bonus */

    /* Style variety bonus: different styles get slight preference */
    uint8_t style_bonus = (uint8_t)((style * 17) % 32);

    uint16_t total = (uint16_t)persona_score + aff_bonus + style_bonus;
    return (uint8_t)(total > 255 ? 255 : total);
}

const char *dating_respond(const char *input) {
    if (!dating_ready) dating_init();
    if (!input) return "안녕하세요!";

    /* Parse input */
    int len = 0;
    while (input[len]) len++;

    uint8_t intensity = (len > 20) ? 77 : 26;
    EmotionIndex stim = EMOTION_JOY;
    if (len > 0) {
        uint8_t c = (uint8_t)input[0];
        stim = (EmotionIndex)(c % 7);
    }

    /* === Multiverse branching: 3 candidate responses === */
    branch_system_init();
    multiverse_init();

    /* Candidate emotions: original, amplified, shifted */
    EmotionVector candidates[3];
    candidates[0] = dating_emotion;  /* as-is */
    candidates[1] = dating_emotion;  /* amplified stimulus */
    candidates[2] = dating_emotion;  /* shifted to adjacent emotion */

    /* Apply stimulus variants */
    emotion_update(&candidates[0], stim, intensity);
    emotion_update(&candidates[1], stim, (uint8_t)(intensity > 127 ? 255 : intensity * 2));
    EmotionIndex shifted = (EmotionIndex)((stim + 1) % 7);
    emotion_update(&candidates[2], shifted, intensity);

    /* Spawn universe per candidate */
    int uids[3];
    for (int i = 0; i < 3; i++) {
        uids[i] = multiverse_spawn(i == 0 ? -1 : 0, i);
        if (uids[i] < 0) {
            /* Fallback: use candidate 0 directly */
            dating_emotion = candidates[0];
            EmotionIndex dom = emotion_dominant(&dating_emotion);
            return responses[dom][persona_get_response_style() % 3];
        }

        /* Write emotion energy into branch canvas for scoring */
        EmotionIndex dom = emotion_dominant(&candidates[i]);
        int style = (persona_get_response_style() + i) % 3;
        uint8_t score = score_candidate(dom, style);

        Cell c;
        memset(&c, 0, sizeof(c));
        c.energy = score;
        c.state  = CELL_ACTIVE;
        c.id     = (uint16_t)(dom * 3 + style);
        branch_apply_patch(uids[i], 512, 512, c);

        /* Score as evidence weight: score mapped to ×256 scale */
        uint16_t evidence = (uint16_t)(score + 128); /* 128-383 range */
        multiverse_probability_update(uids[i], evidence);
    }

    /* Find best universe */
    int best_uid = 0;
    uint16_t best_prob = 0;
    for (int i = 0; i < 3; i++) {
        Universe *u = multiverse_get(uids[i]);
        if (u && u->active && u->probability > best_prob) {
            best_prob = u->probability;
            best_uid  = i;
        }
    }

    /* Collapse to winning universe */
    multiverse_collapse(uids[best_uid]);

    /* Apply winning emotion state */
    dating_emotion = candidates[best_uid];
    persona_update_affection((uint8_t)(intensity / 10));

    /* Record interaction */
    wh_record((uint64_t)len, (const uint8_t *)input,
              (size_t)(len > 63 ? 63 : len));

    /* Apply emotion→constellation feedback */
    constellation_apply_emotion(&dating_emotion);

    /* Select response from winning candidate */
    EmotionIndex dom = emotion_dominant(&dating_emotion);
    int style = (persona_get_response_style() + best_uid) % 3;
    return responses[dom][style];
}

EmotionVector *dating_get_emotion(void) {
    return &dating_emotion;
}

#ifdef DATING_MAIN
int main(void) {
    dating_init();
    char line[256];
    printf("[%s] 안녕하세요! 저는 %s예요. 무슨 이야기를 나눌까요?\n",
           "DATING", persona_get_name());
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        int len = 0;
        while (line[len] && line[len] != '\n') len++;
        line[len] = '\0';
        if (len == 0) continue;
        printf("[ELO] %s\n", dating_respond(line));
    }
    return 0;
}
#endif
