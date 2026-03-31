#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

#define BENCH_ITERATIONS 10000
#define BENCH_RADIUS     4

int main(void) {
    printf("=== Phase B Benchmark: Pattern Recognition ===\n");
    canvas_init();

    /* Seed synthetic data */
    for (int i = 0; i < 1000; i++) {
        Cell c;
        c.color.r = (uint8_t)(i * 13 & 0xFF);
        c.color.g = (uint8_t)(i * 17 & 0xFF);
        c.color.b = (uint8_t)(i * 19 & 0xFF);
        c.color.a = 255;
        c.energy  = (uint8_t)(i % 255);
        c.state   = CELL_ACTIVE;
        c.id      = (uint16_t)i;
        canvas_set((i * 41) % CANVAS_WIDTH, (i * 37) % CANVAS_HEIGHT, c);
    }

    clock_t start = clock();
    int recognized = 0;

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        int x = (i * 41) % CANVAS_WIDTH;
        int y = (i * 37) % CANVAS_HEIGHT;
        PatternResult pr = pattern_recognize(x, y, BENCH_RADIUS);
        if (pr.confidence > 0.0f) recognized++;
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double patterns_per_sec = BENCH_ITERATIONS / (elapsed > 0 ? elapsed : 1e-9);

    printf("Iterations     : %d\n", BENCH_ITERATIONS);
    printf("Recognized     : %d\n", recognized);
    printf("Elapsed        : %.3f s\n", elapsed);
    printf("Patterns/sec   : %.0f\n", patterns_per_sec);
    printf("=== Phase B DONE ===\n");
    return 0;
}
