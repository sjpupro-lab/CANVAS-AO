#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

#define MAX_BRANCHES 64
static Branch branches[MAX_BRANCHES];
static int    branch_count = 0;

void branch_system_init(void) {
    memset(branches, 0, sizeof(branches));
    branch_count = 0;
}

int branch_create(int parent_id) {
    if (branch_count >= MAX_BRANCHES) return -1;
    Branch *b = &branches[branch_count];
    b->id          = branch_count;
    b->parent_id   = parent_id;
    b->diverge_tick = 0;
    b->patch_count = 0;
    b->active      = true;
    return branch_count++;
}

void branch_apply_patch(int branch_id, int x, int y, Cell cell) {
    if (branch_id < 0 || branch_id >= branch_count) return;
    Branch *b = &branches[branch_id];
    if (b->patch_count >= MAX_PATCHES) return;

    /* Update existing patch if present */
    for (int i = 0; i < b->patch_count; i++) {
        if (b->patches[i].x == x && b->patches[i].y == y) {
            b->patches[i].cell = cell;
            return;
        }
    }
    b->patches[b->patch_count].x    = x;
    b->patches[b->patch_count].y    = y;
    b->patches[b->patch_count].cell = cell;
    b->patch_count++;
}

void branch_merge(int src_branch_id, int dst_branch_id) {
    if (src_branch_id < 0 || src_branch_id >= branch_count) return;
    if (dst_branch_id < 0 || dst_branch_id >= branch_count) return;
    Branch *src = &branches[src_branch_id];
    for (int i = 0; i < src->patch_count; i++) {
        branch_apply_patch(dst_branch_id,
                           src->patches[i].x,
                           src->patches[i].y,
                           src->patches[i].cell);
    }
}

Cell branch_get_cell(int branch_id, int x, int y) {
    if (branch_id < 0 || branch_id >= branch_count) return canvas_get(x, y);
    Branch *b = &branches[branch_id];

    /* Check patches (COW) */
    for (int i = 0; i < b->patch_count; i++) {
        if (b->patches[i].x == x && b->patches[i].y == y)
            return b->patches[i].cell;
    }

    /* Fall back to parent */
    if (b->parent_id >= 0 && b->parent_id != branch_id)
        return branch_get_cell(b->parent_id, x, y);

    return canvas_get(x, y);
}

void branch_delete(int branch_id) {
    if (branch_id < 0 || branch_id >= branch_count) return;
    branches[branch_id].active = false;
}

int branch_count_active(void) {
    int cnt = 0;
    for (int i = 0; i < branch_count; i++) {
        if (branches[i].active) cnt++;
    }
    return cnt;
}
