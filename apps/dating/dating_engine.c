#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations from persona.c */
void         persona_load(const char *name);
void         persona_update_affection(float delta);
int          persona_get_response_style(void);
float        persona_get_affection(void);
const char  *persona_get_name(void);

static EmotionVector dating_emotion;
static int           dating_ready = 0;

/* Simple response bank indexed by dominant emotion */
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
    persona_load("ELO");
    emotion_init(&dating_emotion);
    wh_init();
    lane_init();
    constellation_build(512, 512, 8);
    dating_ready = 1;
}

const char *dating_respond(const char *input) {
    if (!dating_ready) dating_init();
    if (!input) return "안녕하세요!";

    /* Use input length and first char as naive sentiment proxy */
    int len = 0;
    while (input[len]) len++;

    float intensity = (len > 20) ? 0.3f : 0.1f;
    EmotionIndex stim = EMOTION_JOY;

    /* Simple keyword detection */
    if (len > 0) {
        uint8_t c = (uint8_t)input[0];
        stim = (EmotionIndex)(c % 7);
    }

    emotion_update(&dating_emotion, stim, intensity);
    persona_update_affection(intensity * 0.1f);

    /* Record interaction */
    wh_record((uint64_t)len, (const uint8_t *)input,
              (size_t)(len > 63 ? 63 : len));

    /* Pick response based on dominant emotion */
    EmotionIndex dom = emotion_dominant(&dating_emotion);
    int style = persona_get_response_style();
    return responses[dom][style % 3];
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
