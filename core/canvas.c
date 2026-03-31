#include "canvasos.h"
#include "cell.h"
#include <string.h>

static Cell canvas[CANVAS_HEIGHT][CANVAS_WIDTH];

void canvas_init(void) {
    memset(canvas, 0, sizeof(canvas));
}

Cell canvas_get(int x, int y) {
    if (x < 0 || x >= CANVAS_WIDTH || y < 0 || y >= CANVAS_HEIGHT) {
        Cell empty = {0};
        return empty;
    }
    return canvas[y][x];
}

void canvas_set(int x, int y, Cell c) {
    if (x < 0 || x >= CANVAS_WIDTH || y < 0 || y >= CANVAS_HEIGHT) return;
    canvas[y][x] = c;
}

void canvas_tick(void) {
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            if (canvas[y][x].state == CELL_ACTIVE) {
                if (canvas[y][x].energy < 255)
                    canvas[y][x].energy++;
            }
        }
    }
}

void canvas_clear(void) {
    memset(canvas, 0, sizeof(canvas));
}
