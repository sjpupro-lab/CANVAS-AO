#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

#define TILE_SIZE 64
#define TILE_COLS (CANVAS_WIDTH  / TILE_SIZE)
#define TILE_ROWS (CANVAS_HEIGHT / TILE_SIZE)

typedef struct {
    uint16_t owner_id;
    uint8_t  flags;
    bool     locked;
} Gate;

static Gate gates[TILE_ROWS][TILE_COLS];

void gate_init(void) {
    memset(gates, 0, sizeof(gates));
}

bool gate_lock(int tile_x, int tile_y, uint16_t owner) {
    if (tile_x < 0 || tile_x >= TILE_COLS || tile_y < 0 || tile_y >= TILE_ROWS)
        return false;
    Gate *g = &gates[tile_y][tile_x];
    if (g->locked) return false;
    g->locked   = true;
    g->owner_id = owner;
    return true;
}

bool gate_unlock(int tile_x, int tile_y, uint16_t owner) {
    if (tile_x < 0 || tile_x >= TILE_COLS || tile_y < 0 || tile_y >= TILE_ROWS)
        return false;
    Gate *g = &gates[tile_y][tile_x];
    if (!g->locked || g->owner_id != owner) return false;
    g->locked   = false;
    g->owner_id = 0;
    return true;
}

bool gate_can_access(int tile_x, int tile_y, uint16_t requester) {
    if (tile_x < 0 || tile_x >= TILE_COLS || tile_y < 0 || tile_y >= TILE_ROWS)
        return false;
    Gate *g = &gates[tile_y][tile_x];
    if (!g->locked) return true;
    return g->owner_id == requester;
}

void gate_isolate_region(int x0, int y0, int x1, int y1, uint16_t owner) {
    int tx0 = x0 / TILE_SIZE;
    int ty0 = y0 / TILE_SIZE;
    int tx1 = x1 / TILE_SIZE;
    int ty1 = y1 / TILE_SIZE;
    for (int ty = ty0; ty <= ty1 && ty < TILE_ROWS; ty++) {
        for (int tx = tx0; tx <= tx1 && tx < TILE_COLS; tx++) {
            gate_lock(tx, ty, owner);
        }
    }
}
