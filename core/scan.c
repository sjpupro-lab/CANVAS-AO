#include "canvasos.h"
#include "cell.h"
#include <stdlib.h>

void scan_ring(int cx, int cy, int radius, ScanCallback cb) {
    if (radius == 0) {
        Cell c = canvas_get(cx, cy);
        cb(cx, cy, &c);
        return;
    }
    int r = radius;
    for (int x = cx - r; x <= cx + r; x++) {
        Cell c;
        c = canvas_get(x, cy - r); cb(x, cy - r, &c);
        c = canvas_get(x, cy + r); cb(x, cy + r, &c);
    }
    for (int y = cy - r + 1; y <= cy + r - 1; y++) {
        Cell c;
        c = canvas_get(cx - r, y); cb(cx - r, y, &c);
        c = canvas_get(cx + r, y); cb(cx + r, y, &c);
    }
}

void scan_spiral(int cx, int cy, int max_radius, ScanCallback cb) {
    for (int r = 0; r <= max_radius; r++) {
        scan_ring(cx, cy, r, cb);
    }
}

int scan_find_pattern(int cx, int cy, int max_r, uint8_t target_energy) {
    for (int r = 0; r <= max_r; r++) {
        if (r == 0) {
            Cell c = canvas_get(cx, cy);
            if (c.energy >= target_energy) return r;
            continue;
        }
        for (int x = cx - r; x <= cx + r; x++) {
            Cell c = canvas_get(x, cy - r);
            if (c.energy >= target_energy) return r;
            c = canvas_get(x, cy + r);
            if (c.energy >= target_energy) return r;
        }
        for (int y = cy - r + 1; y <= cy + r - 1; y++) {
            Cell c = canvas_get(cx - r, y);
            if (c.energy >= target_energy) return r;
            c = canvas_get(cx + r, y);
            if (c.energy >= target_energy) return r;
        }
    }
    return -1;
}

typedef struct { int count; } CountCtx;
static CountCtx _count_ctx;

static void count_cb(int x, int y, Cell *c) {
    (void)x; (void)y;
    if (c->state == CELL_ACTIVE) _count_ctx.count++;
}

int scan_count_active(int cx, int cy, int radius) {
    _count_ctx.count = 0;
    scan_spiral(cx, cy, radius, count_cb);
    return _count_ctx.count;
}
