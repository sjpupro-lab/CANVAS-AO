#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

/* Persona: ELO - balanced personality dating AI */

typedef struct {
    char  name[32];
    float personality_vector[7]; /* joy, trust, fear, surprise, sadness, disgust, anger */
    int   speech_style;          /* 0=casual 1=formal 2=playful */
    float affection_level;       /* 0.0 - 1.0 */
} Persona;

static Persona current_persona;

void persona_load(const char *name) {
    memset(&current_persona, 0, sizeof(current_persona));
    if (!name) name = "ELO";
    int len = 0;
    while (name[len] && len < 31) {
        current_persona.name[len] = name[len];
        len++;
    }
    current_persona.name[len] = '\0';

    /* ELO default: balanced, trusting, playful */
    current_persona.personality_vector[EMOTION_JOY]      = 0.7f;
    current_persona.personality_vector[EMOTION_TRUST]    = 0.8f;
    current_persona.personality_vector[EMOTION_FEAR]     = 0.1f;
    current_persona.personality_vector[EMOTION_SURPRISE] = 0.4f;
    current_persona.personality_vector[EMOTION_SADNESS]  = 0.1f;
    current_persona.personality_vector[EMOTION_DISGUST]  = 0.05f;
    current_persona.personality_vector[EMOTION_ANGER]    = 0.05f;
    current_persona.speech_style   = 2; /* playful */
    current_persona.affection_level = 0.3f;
}

void persona_update_affection(float delta) {
    current_persona.affection_level += delta;
    if (current_persona.affection_level < 0.0f) current_persona.affection_level = 0.0f;
    if (current_persona.affection_level > 1.0f) current_persona.affection_level = 1.0f;
}

int persona_get_response_style(void) {
    return current_persona.speech_style;
}

float persona_get_affection(void) {
    return current_persona.affection_level;
}

const char *persona_get_name(void) {
    return current_persona.name;
}

const float *persona_get_vector(void) {
    return current_persona.personality_vector;
}
