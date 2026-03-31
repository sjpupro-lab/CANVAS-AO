#include "canvasos.h"
#include "cell.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static Constellation g_const;

void constellation_build(int cx, int cy, int radius) {
    memset(&g_const, 0, sizeof(g_const));
    g_const.center_x = cx;
    g_const.center_y = cy;
    int count = 0;

    for (int dy = -radius; dy <= radius && count < MAX_CONSTELLATION_NODES; dy++) {
        for (int dx = -radius; dx <= radius && count < MAX_CONSTELLATION_NODES; dx++) {
            Cell c = canvas_get(cx + dx, cy + dy);
            if (c.state == CELL_ACTIVE || c.energy > 0) {
                g_const.nodes[count].x      = cx + dx;
                g_const.nodes[count].y      = cy + dy;
                g_const.nodes[count].energy = c.energy / 255.0f;
                g_const.nodes[count].conn_count = 0;
                count++;
            }
        }
    }
    g_const.count = count;

    /* Build connections: each node connects to up to 8 nearest neighbors */
    for (int i = 0; i < g_const.count; i++) {
        int nc = 0;
        for (int j = 0; j < g_const.count && nc < 8; j++) {
            if (i == j) continue;
            int dx = g_const.nodes[i].x - g_const.nodes[j].x;
            int dy = g_const.nodes[i].y - g_const.nodes[j].y;
            float dist = sqrtf((float)(dx*dx + dy*dy));
            if (dist <= 2.0f) {
                g_const.nodes[i].connections[nc++] = j;
            }
        }
        g_const.nodes[i].conn_count = nc;
    }
}

void constellation_propagate(int steps) {
    for (int s = 0; s < steps; s++) {
        float new_energy[MAX_CONSTELLATION_NODES];
        for (int i = 0; i < g_const.count; i++) {
            float sum = g_const.nodes[i].energy;
            int   nc  = g_const.nodes[i].conn_count;
            for (int k = 0; k < nc; k++) {
                int j = g_const.nodes[i].connections[k];
                sum += g_const.nodes[j].energy * 0.1f;
            }
            new_energy[i] = sum > 1.0f ? 1.0f : sum;
        }
        for (int i = 0; i < g_const.count; i++) {
            g_const.nodes[i].energy = new_energy[i];
        }
    }
}

int constellation_infer(float query_energy) {
    int   best = -1;
    float best_diff = 1e9f;
    for (int i = 0; i < g_const.count; i++) {
        float diff = fabsf(g_const.nodes[i].energy - query_energy);
        if (diff < best_diff) {
            best_diff = diff;
            best      = i;
        }
    }
    return best;
}

void constellation_update(int x, int y, float delta) {
    for (int i = 0; i < g_const.count; i++) {
        if (g_const.nodes[i].x == x && g_const.nodes[i].y == y) {
            g_const.nodes[i].energy += delta;
            if (g_const.nodes[i].energy < 0.0f) g_const.nodes[i].energy = 0.0f;
            if (g_const.nodes[i].energy > 1.0f) g_const.nodes[i].energy = 1.0f;
            return;
        }
    }
}

Constellation *constellation_get(void) {
    return &g_const;
}
