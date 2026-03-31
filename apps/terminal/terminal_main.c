#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>

static int terminal_ready = 0;

void terminal_init(void) {
    canvas_init();
    lane_init();
    constellation_build(1024, 1024, 8);
    terminal_ready = 1;
}

const char *terminal_process_input(const char *line) {
    if (!terminal_ready) terminal_init();
    if (!line) return "";

    int len = 0;
    while (line[len]) len++;

    /* Map input to canvas region */
    int x = 1024 + (len % 64);
    int y = 1024 + ((len * 7) % 64);

    Cell c;
    c.color.r = (uint8_t)(len & 0xFF);
    c.color.g = (uint8_t)((len >> 1) & 0xFF);
    c.color.b = (uint8_t)((len >> 2) & 0xFF);
    c.color.a = 255;
    c.energy  = (uint8_t)(len % 256);
    c.state   = CELL_ACTIVE;
    c.id      = (uint16_t)(len);
    canvas_set(x, y, c);

    /* Pattern recognition for intent */
    PatternResult pr = pattern_recognize(x, y, 4);

    /* Constellation inference */
    constellation_update(x, y, pr.confidence * 0.2f);
    int best = constellation_infer(pr.confidence);

    /* Simple keyword-based suggestions */
    if (len >= 2 && line[0] == 'g' && line[1] == 'i') return "git status / git log";
    if (len >= 2 && line[0] == 'l' && line[1] == 's') return "ls -la";
    if (len >= 2 && line[0] == 'c' && line[1] == 'd') return "cd <directory>";
    if (len >= 3 && line[0] == 'm' && line[1] == 'a' && line[2] == 'k') return "make all";
    if (len >= 3 && line[0] == 'g' && line[1] == 'c' && line[2] == 'c') return "gcc -O2 -Wall";

    if (best >= 0)
        return "명령어 패턴 인식됨 — 자동완성을 제안합니다.";
    return "입력을 분석 중입니다...";
}

void terminal_main(void) {
    terminal_init();
    char line[512];
    printf("CanvasOS Terminal v%s (A/O 개발자 어시스턴트)\n", AO_VERSION);
    printf("종료: Ctrl+D\n");
    while (1) {
        printf("ao> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        int len = 0;
        while (line[len] && line[len] != '\n') len++;
        line[len] = '\0';
        if (len == 0) continue;
        printf("  → %s\n", terminal_process_input(line));
    }
}

int main(void) {
    terminal_main();
    return 0;
}
