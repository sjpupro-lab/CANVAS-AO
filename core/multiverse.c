/*
 * multiverse.c — Parallel universe management, integer-only (DK-2)
 *
 * Probability: uint16_t 0-65535 (65535 = 1.0).
 * Evidence weight: uint16_t ×256 fixed-point (256 = 1.0x multiplier).
 */

#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

static Universe universes[UNIVERSE_COUNT];
static int      universe_count = 0;

void multiverse_init(void) {
    memset(universes, 0, sizeof(universes));
    universe_count = 0;
}

int multiverse_spawn(int parent_universe, int branch_at_tick) {
    if (universe_count >= UNIVERSE_COUNT) return -1;
    (void)branch_at_tick;

    /* Create a branch for this universe */
    int parent_branch = (parent_universe >= 0 && parent_universe < universe_count)
                        ? universes[parent_universe].branch_id : -1;
    int bid = branch_create(parent_branch);
    if (bid < 0) return -1;

    Universe *u = &universes[universe_count];
    u->id          = universe_count;
    u->branch_id   = bid;
    u->probability = (parent_universe < 0) ? 65535 : 32768;  /* 1.0 or 0.5 */
    u->active      = true;
    return universe_count++;
}

void multiverse_collapse(int universe_id) {
    if (universe_id < 0 || universe_id >= universe_count) return;

    for (int i = 0; i < universe_count; i++) {
        if (i != universe_id) universes[i].active = false;
    }
    universes[universe_id].probability = 65535;
}

Cell multiverse_get_cell(int universe_id, int x, int y) {
    if (universe_id < 0 || universe_id >= universe_count)
        return canvas_get(x, y);
    return branch_get_cell(universes[universe_id].branch_id, x, y);
}

/*
 * Update probability with evidence weight.
 * evidence_weight: uint16_t ×256 fixed-point (256 = 1.0x, 512 = 2.0x, 128 = 0.5x).
 * Then renormalize all active probabilities to sum to 65535.
 */
void multiverse_probability_update(int universe_id, uint16_t evidence_weight) {
    if (universe_id < 0 || universe_id >= universe_count) return;

    /* Apply weight: prob = (prob * evidence) >> 8 */
    uint32_t new_prob = ((uint32_t)universes[universe_id].probability * evidence_weight) >> 8;
    if (new_prob > 65535) new_prob = 65535;
    universes[universe_id].probability = (uint16_t)new_prob;

    /* Renormalize: scale all active probs so sum = 65535 */
    uint32_t total  = 0;
    int      active = 0;
    for (int i = 0; i < universe_count; i++) {
        if (universes[i].active) {
            total += universes[i].probability;
            active++;
        }
    }
    if (total > 0 && active > 0) {
        for (int i = 0; i < universe_count; i++) {
            if (universes[i].active) {
                universes[i].probability =
                    (uint16_t)(((uint32_t)universes[i].probability * 65535 + total / 2) / total);
            }
        }
    }
}

int multiverse_active_count(void) {
    int cnt = 0;
    for (int i = 0; i < universe_count; i++) {
        if (universes[i].active) cnt++;
    }
    return cnt;
}

Universe *multiverse_get(int universe_id) {
    if (universe_id < 0 || universe_id >= universe_count) return NULL;
    return &universes[universe_id];
}
