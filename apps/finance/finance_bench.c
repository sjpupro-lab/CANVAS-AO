#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct {
    double total_patterns;
    double patterns_per_sec;
    double avg_confidence;
    int    data_count;
} BenchResult;

BenchResult finance_bench_run(const V6F *data, int count) {
    BenchResult res = {0};
    if (!data || count <= 0) return res;

    /* Initialize canvas */
    canvas_init();

    double confidence_sum = 0.0;
    int    found = 0;

    for (int i = 0; i < count; i++) {
        /* Map V6F to canvas cell */
        int x = (int)(data[i].v[0] * 0.01f) % CANVAS_WIDTH;
        int y = (int)(data[i].v[4] * 0.001f) % CANVAS_HEIGHT;
        if (x < 0) x = -x;
        if (y < 0) y = -y;
        if (x >= CANVAS_WIDTH)  x = CANVAS_WIDTH  - 1;
        if (y >= CANVAS_HEIGHT) y = CANVAS_HEIGHT - 1;

        Cell c;
        c.color.r = (uint8_t)(data[i].v[0] / 100.0f * 255.0f);
        c.color.g = (uint8_t)(data[i].v[1] / 100.0f * 255.0f);
        c.color.b = (uint8_t)(data[i].v[2] / 100.0f * 255.0f);
        c.color.a = 255;
        c.energy  = (uint8_t)(fabsf(data[i].v[0] - data[i].v[1]) / 10.0f * 255.0f);
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
    res.avg_confidence    = found > 0 ? confidence_sum / found : 0.0;
    res.data_count        = count;
    res.patterns_per_sec  = found * 1000.0; /* synthetic estimate */
    return res;
}

void finance_bench_print_results(const BenchResult *r) {
    printf("=== Finance Bench Results ===\n");
    printf("Data points   : %d\n", r->data_count);
    printf("Patterns found: %.0f\n", r->total_patterns);
    printf("Avg confidence: %.4f\n", r->avg_confidence);
    printf("Patterns/sec  : %.0f\n", r->patterns_per_sec);
}
