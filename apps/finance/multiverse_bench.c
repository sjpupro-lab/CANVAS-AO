/*
 * multiverse_bench.c — Multiverse finance benchmark, integer-only (DK-2)
 *
 * V6F values: int32_t (prices ×100, volumes as-is).
 * Probability: uint16_t 0-65535.
 */

#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int32_t  best_price;     /* ×100 fixed-point */
    int      best_universe;
    uint16_t confidence;     /* 0-65535 */
} MultiversePrediction;

MultiversePrediction multiverse_finance_run(const V6F *data, int count, int num_universes) {
    MultiversePrediction pred = {0};
    if (!data || count <= 0) return pred;

    multiverse_init();

    int n = num_universes < UNIVERSE_COUNT ? num_universes : UNIVERSE_COUNT;

    /* Spawn universes */
    for (int u = 0; u < n; u++) {
        int uid = multiverse_spawn(u == 0 ? -1 : 0, u);
        if (uid < 0) break;

        /* Apply a price offset scenario to each universe (offset ×100) */
        int32_t offset = (u - n / 2) * 50;  /* 0.5 × 100 = 50 */
        for (int i = 0; i < count && i < 16; i++) {
            Cell c;
            int x = (i * 64) % CANVAS_WIDTH;
            int y = (uid * 64) % CANVAS_HEIGHT;
            int32_t adj = data[i].v[0] + offset;
            c.energy  = (uint8_t)(adj > 0 ? (adj > 25500 ? 255 : adj / 100) : 0);
            c.state   = CELL_ACTIVE;
            c.id      = (uint16_t)(uid * 100 + i);
            c.color.r = c.energy;
            c.color.g = c.color.b = c.color.a = 0;
            branch_apply_patch(uid, x, y, c);
        }

        /* Update probability based on trend */
        int32_t trend = data[count - 1].v[0] - data[0].v[0];
        /* evidence = 256 + offset * trend / 2560000  (×256 fixed-point, 256=1.0x) */
        int32_t evidence = 256 + (int32_t)(((int64_t)offset * trend) / 2560000);
        if (evidence < 26) evidence = 26;    /* min ~0.1x */
        if (evidence > 65535) evidence = 65535;
        multiverse_probability_update(uid, (uint16_t)evidence);
    }

    /* Find best universe */
    uint16_t best_prob = 0;
    for (int u = 0; u < n; u++) {
        Universe *uni = multiverse_get(u);
        if (uni && uni->active && uni->probability > best_prob) {
            best_prob          = uni->probability;
            pred.best_universe = u;
        }
    }

    /* best_price = last_price * (1.0 + best_prob/65535 * 0.01) */
    /* = last_price + last_price * best_prob / 6553500 */
    pred.best_price = data[count - 1].v[0] +
                      (int32_t)(((int64_t)data[count - 1].v[0] * best_prob) / 6553500);
    pred.confidence = best_prob;
    return pred;
}

void multiverse_bench_print(const MultiversePrediction *p) {
    printf("=== Multiverse Finance Bench ===\n");
    printf("Best universe  : %d\n", p->best_universe);
    printf("Predicted price: %d (x100)\n", p->best_price);
    printf("Confidence     : %u/65535\n", p->confidence);
}

#ifdef FINANCE_MAIN
int main(void) {
    /* Synthetic price data (×100 fixed-point) */
    V6F data[32];
    for (int i = 0; i < 32; i++) {
        data[i] = v6f_encode(10000 + i * 50,     /* price: $100.00 + $0.50*i */
                             10000 + i * 40,      /* open */
                             10100 + i * 50,      /* high */
                             9950 + i * 40,       /* low */
                             100000 + i * 1000,   /* volume */
                             i);                  /* timestamp */
    }

    /* Multiverse bench */
    MultiversePrediction mp = multiverse_finance_run(data, 32, 8);
    multiverse_bench_print(&mp);
    return 0;
}
#endif
