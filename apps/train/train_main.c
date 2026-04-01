/*
 * train_main.c — CLI training tool for CANVAS-AO
 *
 * Usage:
 *   ./bin/train corpus.txt                    # 일반 텍스트 훈련 (chat + lang)
 *   ./bin/train --emotion joy happy.txt       # 감정 라벨 훈련
 *   ./bin/train --emotion anger angry.txt     # 감정 라벨 훈련
 *   ./bin/train file1.txt file2.txt           # 여러 파일
 *   ./bin/train --emotion trust f1.txt f2.txt # 감정 + 여러 파일
 *   ./bin/train --info                        # 훈련 상태 조회
 *
 * 감정 라벨: joy, trust, fear, surprise, sadness, disgust, anger
 * 파일은 4KB 청크로 읽어서 chat_train() + pattern_lang_train() 호출.
 * --emotion 옵션 시 emotion_detect_train() 추가 호출.
 */

#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHUNK_SIZE 4096

static const char *emotion_names[7] = {
    "joy", "trust", "fear", "surprise", "sadness", "disgust", "anger"
};

static int parse_emotion(const char *name) {
    for (int i = 0; i < 7; i++) {
        if (strcmp(name, emotion_names[i]) == 0) return i;
    }
    return -1;
}

static void print_usage(void) {
    printf("CANVAS-AO Training Tool\n\n");
    printf("Usage:\n");
    printf("  ./bin/train <file> [file2...]           Train chat + language patterns\n");
    printf("  ./bin/train --emotion <label> <file>... Train emotion detection\n");
    printf("  ./bin/train --info                      Show training status\n");
    printf("\nEmotion labels: joy, trust, fear, surprise, sadness, disgust, anger\n");
    printf("\nExamples:\n");
    printf("  ./bin/train corpus_ko.txt\n");
    printf("  ./bin/train --emotion joy happy_data.txt\n");
    printf("  ./bin/train --emotion anger angry1.txt angry2.txt\n");
}

static void print_info(void) {
    printf("=== Training Status ===\n");
    printf("Chat vocabulary : %d words\n", chat_word_count());
    printf("Lang patterns   : %d bytes trained\n", pattern_lang_trained_count());
    printf("Emotion training:\n");
    for (int i = 0; i < 7; i++) {
        int cnt = emotion_detect_trained((EmotionIndex)i);
        printf("  %-10s: %d bytes\n", emotion_names[i], cnt);
    }
}

static int train_file(const char *path, int emotion_idx) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open '%s'\n", path);
        return -1;
    }

    char buf[CHUNK_SIZE];
    size_t total = 0;

    while (1) {
        size_t n = fread(buf, 1, CHUNK_SIZE, fp);
        if (n == 0) break;

        /* Chat + language pattern training */
        chat_train(buf, (int)n);

        /* Emotion training if label specified */
        if (emotion_idx >= 0) {
            emotion_detect_train((const uint8_t *)buf, (int)n,
                                 (EmotionIndex)emotion_idx);
        }

        total += n;
    }
    fclose(fp);

    printf("  %-40s %zu bytes", path, total);
    if (emotion_idx >= 0) printf(" [%s]", emotion_names[emotion_idx]);
    printf("\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    /* Initialize systems */
    chat_init();
    pattern_lang_init();
    emotion_detect_init();

    /* Parse arguments */
    if (strcmp(argv[1], "--info") == 0) {
        print_info();
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }

    int emotion_idx = -1;
    int file_start = 1;

    if (strcmp(argv[1], "--emotion") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: --emotion requires <label> and <file>\n");
            print_usage();
            return 1;
        }
        emotion_idx = parse_emotion(argv[2]);
        if (emotion_idx < 0) {
            fprintf(stderr, "Error: unknown emotion '%s'\n", argv[2]);
            printf("Valid labels: ");
            for (int i = 0; i < 7; i++) printf("%s ", emotion_names[i]);
            printf("\n");
            return 1;
        }
        file_start = 3;
    }

    printf("=== CANVAS-AO Training ===\n");
    if (emotion_idx >= 0) printf("Emotion label: %s\n", emotion_names[emotion_idx]);
    printf("Files:\n");

    int files_ok = 0, files_err = 0;
    for (int i = file_start; i < argc; i++) {
        if (train_file(argv[i], emotion_idx) == 0)
            files_ok++;
        else
            files_err++;
    }

    printf("\n=== Result ===\n");
    printf("Files trained: %d ok, %d failed\n", files_ok, files_err);
    print_info();

    return files_err > 0 ? 1 : 0;
}
