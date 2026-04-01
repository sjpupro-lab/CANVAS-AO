/*
 * elo.c — Energy Layer Oscillation: integer-only precision cascade
 *
 * Three 256x256 canvas layers at cascading time scales:
 *   Layer 3 (100x): byte-pair patterns, cascades every 100 bytes
 *   Layer 2 (10x):  quad-gram patterns, cascades every 1000 bytes
 *   Layer 1 (1x):   hex-gram patterns, accumulates over document
 *
 * Clock analogy:
 *   Layer 3 = second hand (fast, fine)
 *   Layer 2 = minute hand (medium)
 *   Layer 1 = hour hand (slow, coarse)
 *
 * Cascade: fast layer accuracy → slow layer trust boost.
 * "The pattern of the fast hand becomes the value of the slow hand."
 *
 * All operations integer-only (DK-2). Memory: 3 x 192KB = 576KB.
 */

#include "canvasos.h"
#include <string.h>

#define ELO_DIM       256
#define ELO_L3_PERIOD 100   /* L3 cascade interval (bytes)  */
#define ELO_L2_PERIOD 10    /* L2 cascade interval (L2-ticks = L3_PERIOD * L2_PERIOD bytes) */

/* Per-cell: prediction + confidence + energy. All uint8_t. */
typedef struct {
    uint8_t pred;     /* predicted next byte              */
    uint8_t conf;     /* prediction confidence (0-255)    */
    uint8_t energy;   /* access frequency (saturating)    */
} ECell;

/* Layer state */
typedef struct {
    ECell    cells[ELO_DIM][ELO_DIM];
    uint32_t tick;            /* ticks since last cascade       */
    uint32_t period_hits;     /* correct predictions this period */
    uint32_t period_total;    /* total predictions this period   */
    uint8_t  trust;           /* temporal trust score (0-255)    */
} ELayer;

static ELayer g_l3;   /* 100x speed: orders 1-2 */
static ELayer g_l2;   /* 10x speed:  orders 3-4 */
static ELayer g_l1;   /* 1x speed:   orders 5-6 */

/* FNV-1a hash (same as compress.c n-gram) */
static uint32_t elo_fnv(const uint8_t *data, int len) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

/* Map context to 256x256 cell coordinates */
static void elo_map(const uint8_t *ctx, int ctx_len, int max_order,
                    int *out_x, int *out_y) {
    int use = max_order < ctx_len ? max_order : ctx_len;
    if (use < 1) { *out_x = 0; *out_y = 0; return; }
    uint32_t h = elo_fnv(ctx + ctx_len - use, use);
    *out_x = (int)(h & 0xFF);
    *out_y = (int)((h >> 8) & 0xFF);
}

/* Update a single layer cell with actual byte */
static void layer_update(ELayer *l, int x, int y, uint8_t actual) {
    ECell *c = &l->cells[y][x];

    l->period_total++;

    if (c->pred == actual) {
        /* Correct prediction: confidence++ */
        if (c->conf < 255) c->conf++;
        l->period_hits++;
    } else {
        /* Wrong prediction: confidence-- or replace */
        if (c->conf > 1) {
            c->conf--;
        } else {
            c->pred = actual;
            c->conf = 1;
        }
    }

    /* Energy: tracks how often this cell is accessed */
    if (c->energy < 255) c->energy++;
}

/*
 * Cascade: fast layer's accuracy over its period updates trust scores.
 *
 * "100 ticks of the second hand" → one update to the minute hand.
 * The "pattern" (hit rate) of the fast layer becomes the slow layer's
 * trust adjustment — precision through temporal resolution, not floats.
 */
static void do_cascade(ELayer *fast, ELayer *slow) {
    /* Compute hit rate as integer 0-255 */
    uint8_t hit_rate = 0;
    if (fast->period_total > 0) {
        hit_rate = (uint8_t)((fast->period_hits * 255u) / fast->period_total);
    }

    /* Update fast layer trust: EMA = (trust * 3 + hit_rate) >> 2 */
    fast->trust = (uint8_t)(((uint16_t)fast->trust * 3 + hit_rate + 2) >> 2);

    /* Cascade effect: fast accuracy boosts/decays slow trust
     * Very accurate fast layer → slow layer gains confidence
     * Inaccurate fast layer → slow layer loses confidence */
    if (hit_rate > 160) {
        uint8_t boost = (hit_rate - 160) >> 3;  /* 0-11 */
        if (slow->trust <= 255 - boost)
            slow->trust += boost;
        else
            slow->trust = 255;
    } else if (hit_rate < 64) {
        uint8_t decay = (64 - hit_rate) >> 4;   /* 0-4 */
        if (slow->trust >= decay)
            slow->trust -= decay;
        else
            slow->trust = 0;
    }

    /* Reset fast layer period counters for next cascade interval */
    fast->period_hits  = 0;
    fast->period_total = 0;
}

/* ===== Public API ===== */

void elo_init(void) {
    memset(&g_l3, 0, sizeof(ELayer));
    memset(&g_l2, 0, sizeof(ELayer));
    memset(&g_l1, 0, sizeof(ELayer));
    /* Start at 50% trust — let cascade adapt from neutral */
    g_l3.trust = 128;
    g_l2.trust = 128;
    g_l1.trust = 128;
}

uint8_t elo_predict(const uint8_t *ctx, int ctx_len) {
    int x, y;

    /* Layer 3: order 2 context, trust-weighted */
    elo_map(ctx, ctx_len, 2, &x, &y);
    uint32_t w3 = (uint32_t)g_l3.trust * g_l3.cells[y][x].conf;
    uint8_t  p3 = g_l3.cells[y][x].pred;

    /* Layer 2: order 4 context */
    elo_map(ctx, ctx_len, 4, &x, &y);
    uint32_t w2 = (uint32_t)g_l2.trust * g_l2.cells[y][x].conf;
    uint8_t  p2 = g_l2.cells[y][x].pred;

    /* Layer 1: order 6 context */
    elo_map(ctx, ctx_len, 6, &x, &y);
    uint32_t w1 = (uint32_t)g_l1.trust * g_l1.cells[y][x].conf;
    uint8_t  p1 = g_l1.cells[y][x].pred;

    /* Highest trust*confidence wins */
    if (w3 >= w2 && w3 >= w1) return p3;
    if (w2 >= w1) return p2;
    return p1;
}

uint8_t elo_confidence(const uint8_t *ctx, int ctx_len) {
    int x, y;

    elo_map(ctx, ctx_len, 2, &x, &y);
    uint32_t w3 = (uint32_t)g_l3.trust * g_l3.cells[y][x].conf;
    uint8_t  c3 = g_l3.cells[y][x].conf;

    elo_map(ctx, ctx_len, 4, &x, &y);
    uint32_t w2 = (uint32_t)g_l2.trust * g_l2.cells[y][x].conf;
    uint8_t  c2 = g_l2.cells[y][x].conf;

    elo_map(ctx, ctx_len, 6, &x, &y);
    uint32_t w1 = (uint32_t)g_l1.trust * g_l1.cells[y][x].conf;
    uint8_t  c1 = g_l1.cells[y][x].conf;

    if (w3 >= w2 && w3 >= w1) return c3;
    if (w2 >= w1) return c2;
    return c1;
}

void elo_feed(const uint8_t *ctx, int ctx_len, uint8_t actual) {
    int x, y;

    /* Update all three layers at their respective context orders */
    elo_map(ctx, ctx_len, 2, &x, &y);
    layer_update(&g_l3, x, y, actual);

    elo_map(ctx, ctx_len, 4, &x, &y);
    layer_update(&g_l2, x, y, actual);

    elo_map(ctx, ctx_len, 6, &x, &y);
    layer_update(&g_l1, x, y, actual);

    /* Cascade: L3 → L2 every ELO_L3_PERIOD ticks */
    g_l3.tick++;
    if (g_l3.tick >= ELO_L3_PERIOD) {
        do_cascade(&g_l3, &g_l2);
        g_l3.tick = 0;

        /* L2 → L1 every ELO_L2_PERIOD L2-cascade events
         * = every ELO_L3_PERIOD * ELO_L2_PERIOD bytes (1000) */
        g_l2.tick++;
        if (g_l2.tick >= ELO_L2_PERIOD) {
            do_cascade(&g_l2, &g_l1);
            g_l2.tick = 0;
        }
    }
}

uint8_t elo_get_trust(int layer) {
    switch (layer) {
        case 3: return g_l3.trust;
        case 2: return g_l2.trust;
        case 1: return g_l1.trust;
        default: return 128;
    }
}
