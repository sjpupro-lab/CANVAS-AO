#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>

#define CRAFT_VIEW_W 64
#define CRAFT_VIEW_H 32

static int craft_ready = 0;
static uint64_t craft_tick_count = 0;

void craft_init(void) {
    canvas_init();
    lane_init();

    /* Seed some active cells */
    for (int i = 0; i < 100; i++) {
        Cell c;
        c.color.r = (uint8_t)(i * 7);
        c.color.g = (uint8_t)(i * 13);
        c.color.b = (uint8_t)(i * 17);
        c.color.a = 255;
        c.energy  = (uint8_t)(i % 200 + 20);
        c.state   = CELL_ACTIVE;
        c.id      = (uint16_t)i;
        canvas_set((i * 41) % CANVAS_WIDTH, (i * 37) % CANVAS_HEIGHT, c);
    }
    craft_ready = 1;
}

void craft_render_canvas(void) {
    /* ASCII visualization of top-left CRAFT_VIEW_W x CRAFT_VIEW_H region */
    printf("\033[2J\033[H"); /* clear screen */
    printf("CanvasCraft — tick %llu\n", (unsigned long long)craft_tick_count);
    for (int y = 0; y < CRAFT_VIEW_H; y++) {
        for (int x = 0; x < CRAFT_VIEW_W; x++) {
            Cell c = canvas_get(x * (CANVAS_WIDTH / CRAFT_VIEW_W),
                                y * (CANVAS_HEIGHT / CRAFT_VIEW_H));
            if (c.state == CELL_ACTIVE && c.energy > 128) putchar('#');
            else if (c.state == CELL_ACTIVE)               putchar('.');
            else if (c.energy > 0)                         putchar('+');
            else                                           putchar(' ');
        }
        putchar('\n');
    }
    fflush(stdout);
}

void craft_tick(void) {
    if (!craft_ready) craft_init();
    canvas_tick();
    craft_tick_count++;

    /* Spread energy from active cells to neighbors */
    for (int i = 0; i < 10; i++) {
        int x = (int)(craft_tick_count * 37 + i * 41) % CANVAS_WIDTH;
        int y = (int)(craft_tick_count * 29 + i * 53) % CANVAS_HEIGHT;
        Cell c = canvas_get(x, y);
        if (c.state == CELL_ACTIVE && c.energy > 10) {
            Cell neighbor = canvas_get((x + 1) % CANVAS_WIDTH, y);
            if (neighbor.state == CELL_IDLE) {
                neighbor.state  = CELL_ACTIVE;
                neighbor.energy = c.energy / 2;
                neighbor.color  = c.color;
                canvas_set((x + 1) % CANVAS_WIDTH, y, neighbor);
            }
        }
    }
}

void craft_main(void) {
    craft_init();
    for (int i = 0; i < 10; i++) {
        craft_tick();
        craft_render_canvas();
    }
}

int main(void) {
    craft_main();
    return 0;
}
