/*
 * constellation.c — Graph-based energy inference, integer-only (DK-2)
 *
 * Node energy: uint8_t 0-255 (mapped from original 0.0-1.0 float).
 * Propagation: neighbor energy contributes 1/16 (~0.0625, close to original 0.1).
 * Distance uses squared integer comparison (no sqrtf).
 */

#include "canvasos.h"
#include "cell.h"
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
                g_const.nodes[count].energy  = c.energy;  /* already 0-255 */
                g_const.nodes[count].conn_count = 0;
                count++;
            }
        }
    }
    g_const.count = count;

    /* Build connections: each node connects to up to 8 nearest neighbors */
    /* Use squared distance <= 4 (was dist <= 2.0f → dist² <= 4) */
    for (int i = 0; i < g_const.count; i++) {
        int nc = 0;
        for (int j = 0; j < g_const.count && nc < 8; j++) {
            if (i == j) continue;
            int dx = g_const.nodes[i].x - g_const.nodes[j].x;
            int dy = g_const.nodes[i].y - g_const.nodes[j].y;
            int dist_sq = dx * dx + dy * dy;
            if (dist_sq <= 4) {
                g_const.nodes[i].connections[nc++] = j;
            }
        }
        g_const.nodes[i].conn_count = nc;
    }
}

void constellation_propagate(int steps) {
    for (int s = 0; s < steps; s++) {
        uint8_t new_energy[MAX_CONSTELLATION_NODES];
        for (int i = 0; i < g_const.count; i++) {
            uint16_t sum = g_const.nodes[i].energy;
            int nc = g_const.nodes[i].conn_count;
            for (int k = 0; k < nc; k++) {
                int j = g_const.nodes[i].connections[k];
                /* Contribute neighbor_energy / 16 ≈ 0.0625 (was 0.1f) */
                sum += (uint16_t)g_const.nodes[j].energy >> 4;
            }
            new_energy[i] = (uint8_t)(sum > 255 ? 255 : sum);
        }
        for (int i = 0; i < g_const.count; i++) {
            g_const.nodes[i].energy = new_energy[i];
        }
    }
}

int constellation_infer(uint8_t query_energy) {
    int      best      = -1;
    uint16_t best_diff = 256;
    for (int i = 0; i < g_const.count; i++) {
        int d = (int)g_const.nodes[i].energy - (int)query_energy;
        uint16_t diff = (uint16_t)(d < 0 ? -d : d);
        if (diff < best_diff) {
            best_diff = diff;
            best      = i;
        }
    }
    return best;
}

void constellation_update(int x, int y, int16_t delta) {
    for (int i = 0; i < g_const.count; i++) {
        if (g_const.nodes[i].x == x && g_const.nodes[i].y == y) {
            int16_t val = (int16_t)g_const.nodes[i].energy + delta;
            if (val < 0)   val = 0;
            if (val > 255) val = 255;
            g_const.nodes[i].energy = (uint8_t)val;
            return;
        }
    }
}

Constellation *constellation_get(void) {
    return &g_const;
}
