#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_DATA_SIZE (64 * 1024) /* 64 KB per test */

typedef struct {
    const char *name;
    float       ratio;
    double      speed_mb;
} PhaseResult;

static PhaseResult bench_one(const char *name, const uint8_t *data, size_t size) {
    PhaseResult r;
    r.name = name;

    uint8_t *out = (uint8_t *)malloc(size + size / 4 + 16);
    if (!out) { r.ratio = 0; r.speed_mb = 0; return r; }

    clock_t t0 = clock();
    int csz = compress_predicted_delta(data, size, out, size + size / 4 + 16);
    clock_t t1 = clock();

    if (csz > 0) {
        r.ratio    = compress_ratio(size, (size_t)csz);
        double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
        r.speed_mb = elapsed > 0 ? (size / 1e6) / elapsed : 0.0;
    } else {
        r.ratio    = 0.0f;
        r.speed_mb = 0.0;
    }
    free(out);
    return r;
}

int main(void) {
    printf("=== Phase D Compression Benchmark ===\n");

    uint8_t *data = (uint8_t *)malloc(BENCH_DATA_SIZE);
    if (!data) { fprintf(stderr, "malloc failed\n"); return 1; }

    PhaseResult results[3];

    /* 1. Random data (worst case) */
    srand(42);
    for (int i = 0; i < BENCH_DATA_SIZE; i++) data[i] = (uint8_t)(rand() & 0xFF);
    results[0] = bench_one("Random", data, BENCH_DATA_SIZE);

    /* 2. Repetitive data (best case) */
    for (int i = 0; i < BENCH_DATA_SIZE; i++) data[i] = (uint8_t)(i % 8);
    results[1] = bench_one("Repetitive", data, BENCH_DATA_SIZE);

    /* 3. Natural language-like (medium) */
    const char *words = "the canvas thinks and execution is thinking ";
    size_t wlen = 0;
    while (words[wlen]) wlen++;
    for (int i = 0; i < BENCH_DATA_SIZE; i++) data[i] = (uint8_t)words[i % wlen];
    results[2] = bench_one("NaturalLang", data, BENCH_DATA_SIZE);

    printf("%-16s  %8s  %12s\n", "Type", "Ratio", "Speed(MB/s)");
    printf("%-16s  %8s  %12s\n", "----", "-----", "-----------");
    for (int i = 0; i < 3; i++) {
        printf("%-16s  %8.3f  %12.2f\n",
               results[i].name, results[i].ratio, results[i].speed_mb);
    }
    printf("=== Phase D DONE ===\n");
    free(data);
    return 0;
}
