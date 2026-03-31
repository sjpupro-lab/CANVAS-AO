#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

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
    u->probability = (parent_universe < 0) ? 1.0f : 0.5f;
    u->active      = true;
    return universe_count++;
}

void multiverse_collapse(int universe_id) {
    if (universe_id < 0 || universe_id >= universe_count) return;

    /* Apply this universe's branch to canvas */
    Branch *b = NULL;
    (void)b;

    for (int i = 0; i < universe_count; i++) {
        if (i != universe_id) universes[i].active = false;
    }
    universes[universe_id].probability = 1.0f;
}

Cell multiverse_get_cell(int universe_id, int x, int y) {
    if (universe_id < 0 || universe_id >= universe_count)
        return canvas_get(x, y);
    return branch_get_cell(universes[universe_id].branch_id, x, y);
}

void multiverse_probability_update(int universe_id, float evidence_weight) {
    if (universe_id < 0 || universe_id >= universe_count) return;
    universes[universe_id].probability *= evidence_weight;
    /* Renormalize */
    float total = 0.0f;
    int   active = 0;
    for (int i = 0; i < universe_count; i++) {
        if (universes[i].active) {
            total += universes[i].probability;
            active++;
        }
    }
    if (total > 0.0f && active > 0) {
        for (int i = 0; i < universe_count; i++) {
            if (universes[i].active)
                universes[i].probability /= total;
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
