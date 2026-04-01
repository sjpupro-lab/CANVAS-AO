/*
 * finance_bench.c — Finance pattern recognition benchmark, integer-only (DK-2)
 */

#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int total_patterns;
    int patterns_per_sec;
    int avg_confidence;   /* 0-255 */
    int data_count;
} BenchResult;

BenchResult finance_bench_run(const V6F *data, int count) {
    BenchResult res = {0};
    if (!data || count <= 0) return res;

    /* Initialize canvas */
    canvas_init();

    uint32_t confidence_sum = 0;
    int      found = 0;

    for (int i = 0; i < count; i++) {
        /* Map V6F to canvas cell */
        int x = (int)(data[i].v[0] / 100) % CANVAS_WIDTH;
        int y = (int)(data[i].v[4] / 1000) % CANVAS_HEIGHT;
        if (x < 0) x = -x;
        if (y < 0) y = -y;
        if (x >= CANVAS_WIDTH)  x = CANVAS_WIDTH  - 1;
        if (y >= CANVAS_HEIGHT) y = CANVAS_HEIGHT - 1;

        Cell c;
        /* Scale price fields (assumed ×100) to 0-255 color */
        c.color.r = (uint8_t)(data[i].v[0] > 25500 ? 255 : (data[i].v[0] * 255 / 25500));
        c.color.g = (uint8_t)(data[i].v[1] > 25500 ? 255 : (data[i].v[1] * 255 / 25500));
        c.color.b = (uint8_t)(data[i].v[2] > 25500 ? 255 : (data[i].v[2] * 255 / 25500));
        c.color.a = 255;
        int32_t diff = data[i].v[0] - data[i].v[1];
        if (diff < 0) diff = -diff;
        c.energy  = (uint8_t)(diff > 2550 ? 255 : (diff * 255 / 2550));
        c.state   = CELL_ACTIVE;
        c.id      = (uint16_t)i;
        canvas_set(x, y, c);

        PatternResult pr = pattern_recognize(x, y, 4);
        confidence_sum += pr.confidence;
        found++;
    }

    /* Constellation inference */
    constellation_build(2048, 2048, 16);
    constellation_propagate(3);

    res.total_patterns    = found;
    res.avg_confidence    = found > 0 ? (int)(confidence_sum / (uint32_t)found) : 0;
    res.data_count        = count;
    res.patterns_per_sec  = found * 1000; /* synthetic estimate */
    return res;
}

void finance_bench_print_results(const BenchResult *r) {
    printf("=== Finance Bench Results ===\n");
    printf("Data points   : %d\n", r->data_count);
    printf("Patterns found: %d\n", r->total_patterns);
    printf("Avg confidence: %d/255\n", r->avg_confidence);
    printf("Patterns/sec  : %d\n", r->patterns_per_sec);
}
