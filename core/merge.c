#include "canvasos.h"
#include "cell.h"
#include <string.h>

static Cell merge_two(Cell a, Cell b, MergePolicy policy) {
    Cell out;
    switch (policy) {
    case MERGE_LATEST:
        out = b;
        break;
    case MERGE_OLDEST:
        out = a;
        break;
    case MERGE_AND:
        out.color.a = a.color.a & b.color.a;
        out.color.b = a.color.b & b.color.b;
        out.color.g = a.color.g & b.color.g;
        out.color.r = a.color.r & b.color.r;
        out.energy  = a.energy  & b.energy;
        out.state   = a.state   & b.state;
        out.id      = a.id      & b.id;
        break;
    case MERGE_OR:
        out.color.a = a.color.a | b.color.a;
        out.color.b = a.color.b | b.color.b;
        out.color.g = a.color.g | b.color.g;
        out.color.r = a.color.r | b.color.r;
        out.energy  = a.energy  | b.energy;
        out.state   = (a.state > b.state) ? a.state : b.state;
        out.id      = a.id      | b.id;
        break;
    case MERGE_XOR:
        out.color.a = a.color.a ^ b.color.a;
        out.color.b = a.color.b ^ b.color.b;
        out.color.g = a.color.g ^ b.color.g;
        out.color.r = a.color.r ^ b.color.r;
        out.energy  = a.energy  ^ b.energy;
        out.state   = CELL_IDLE;
        out.id      = a.id      ^ b.id;
        break;
    case MERGE_AVERAGE:
        out.color.a = (uint8_t)(((int)a.color.a + b.color.a) / 2);
        out.color.b = (uint8_t)(((int)a.color.b + b.color.b) / 2);
        out.color.g = (uint8_t)(((int)a.color.g + b.color.g) / 2);
        out.color.r = (uint8_t)(((int)a.color.r + b.color.r) / 2);
        out.energy  = (uint8_t)(((int)a.energy  + b.energy)  / 2);
        out.state   = a.state;
        out.id      = (uint16_t)(((int)a.id + b.id) / 2);
        break;
    default:
        out = b;
        break;
    }
    return out;
}

Cell merge_cells(Cell a, Cell b, MergePolicy policy) {
    return merge_two(a, b, policy);
}

void merge_region(int x0, int y0, int x1, int y1, MergePolicy policy) {
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (x + 1 <= x1) {
                Cell a = canvas_get(x, y);
                Cell b = canvas_get(x + 1, y);
                Cell m = merge_two(a, b, policy);
                canvas_set(x, y, m);
            }
        }
    }
}

Cell merge_apply_delta(Cell base, Cell delta) {
    Cell out;
    out.color.a = base.color.a ^ delta.color.a;
    out.color.b = base.color.b ^ delta.color.b;
    out.color.g = base.color.g ^ delta.color.g;
    out.color.r = base.color.r ^ delta.color.r;
    out.energy  = (uint8_t)((base.energy + delta.energy) > 255 ? 255 :
                             base.energy + delta.energy);
    out.state   = delta.state != CELL_IDLE ? delta.state : base.state;
    out.id      = delta.id != 0 ? delta.id : base.id;
    return out;
}

Cell merge_conflict_resolve(const Cell *candidates, int count, MergePolicy policy) {
    if (count <= 0) { Cell e = {0}; return e; }
    Cell result = candidates[0];
    for (int i = 1; i < count; i++) {
        result = merge_two(result, candidates[i], policy);
    }
    return result;
}
