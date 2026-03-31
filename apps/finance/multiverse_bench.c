#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    float best_price;
    int   best_universe;
    float confidence;
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

        /* Apply a price offset scenario to each universe */
        float offset = (u - n / 2) * 0.5f;
        for (int i = 0; i < count && i < 16; i++) {
            Cell c;
            int x = (i * 64) % CANVAS_WIDTH;
            int y = (uid * 64) % CANVAS_HEIGHT;
            c.energy  = (uint8_t)((data[i].v[0] + offset) * 0.5f);
            c.state   = CELL_ACTIVE;
            c.id      = (uint16_t)(uid * 100 + i);
            c.color.r = c.energy;
            c.color.g = c.color.b = c.color.a = 0;
            branch_apply_patch(uid, x, y, c);
        }

        /* Update probability based on trend */
        float trend = data[count - 1].v[0] - data[0].v[0];
        float evidence = 1.0f + offset * trend * 0.01f;
        if (evidence < 0.1f) evidence = 0.1f;
        multiverse_probability_update(uid, evidence);
    }

    /* Find best universe */
    float best_prob = -1.0f;
    for (int u = 0; u < n; u++) {
        Universe *uni = multiverse_get(u);
        if (uni && uni->active && uni->probability > best_prob) {
            best_prob          = uni->probability;
            pred.best_universe = u;
        }
    }

    pred.best_price = data[count - 1].v[0] * (1.0f + best_prob * 0.01f);
    pred.confidence = best_prob;
    return pred;
}

void multiverse_bench_print(const MultiversePrediction *p) {
    printf("=== Multiverse Finance Bench ===\n");
    printf("Best universe : %d\n", p->best_universe);
    printf("Predicted price: %.2f\n", p->best_price);
    printf("Confidence    : %.4f\n", p->confidence);
}

#ifdef FINANCE_MAIN
int main(void) {
    /* Synthetic price data */
    V6F data[32];
    for (int i = 0; i < 32; i++) {
        data[i] = v6f_encode(100.0f + i * 0.5f, 100.0f + i * 0.4f,
                             101.0f + i * 0.5f, 99.5f + i * 0.4f,
                             1000.0f + i * 10.0f, (float)i);
    }

    /* Multiverse bench */
    MultiversePrediction mp = multiverse_finance_run(data, 32, 8);
    multiverse_bench_print(&mp);
    return 0;
}
#endif
