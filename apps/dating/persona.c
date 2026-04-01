/*
 * persona.c — Dating AI personality, integer-only (DK-2)
 *
 * personality_vector: uint8_t[7] (0-255, mapped from 0.0-1.0).
 * affection_level: uint8_t (0-255).
 */

#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    char    name[32];
    uint8_t personality_vector[7]; /* joy, trust, fear, surprise, sadness, disgust, anger */
    int     speech_style;          /* 0=casual 1=formal 2=playful */
    uint8_t affection_level;       /* 0-255 */
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
    current_persona.personality_vector[EMOTION_JOY]      = 179;  /* 0.7 * 255 */
    current_persona.personality_vector[EMOTION_TRUST]    = 204;  /* 0.8 * 255 */
    current_persona.personality_vector[EMOTION_FEAR]     = 26;   /* 0.1 * 255 */
    current_persona.personality_vector[EMOTION_SURPRISE] = 102;  /* 0.4 * 255 */
    current_persona.personality_vector[EMOTION_SADNESS]  = 26;   /* 0.1 * 255 */
    current_persona.personality_vector[EMOTION_DISGUST]  = 13;   /* 0.05 * 255 */
    current_persona.personality_vector[EMOTION_ANGER]    = 13;   /* 0.05 * 255 */
    current_persona.speech_style    = 2;   /* playful */
    current_persona.affection_level = 77;  /* 0.3 * 255 */
}

void persona_update_affection(uint8_t delta) {
    uint16_t sum = (uint16_t)current_persona.affection_level + delta;
    current_persona.affection_level = (uint8_t)(sum > 255 ? 255 : sum);
}

int persona_get_response_style(void) {
    return current_persona.speech_style;
}

uint8_t persona_get_affection(void) {
    return current_persona.affection_level;
}

const char *persona_get_name(void) {
    return current_persona.name;
}

const uint8_t *persona_get_vector(void) {
    return current_persona.personality_vector;
}
