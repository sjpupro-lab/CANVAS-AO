#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

static Lane lanes[LANE_COUNT];
static int  lane_initialized = 0;

void lane_init(void) {
    memset(lanes, 0, sizeof(lanes));
    for (int i = 0; i < LANE_COUNT; i++) lanes[i].id = (uint8_t)i;
    lane_initialized = 1;
}

int lane_spawn(TaskFn task_fn, void *context, uint8_t priority) {
    for (int i = 0; i < LANE_COUNT; i++) {
        if (!lanes[i].active) {
            lanes[i].active   = true;
            lanes[i].task_fn  = task_fn;
            lanes[i].context  = context;
            lanes[i].priority = priority;
            lanes[i].ticks    = 0;
            return i;
        }
    }
    return -1;
}

void lane_kill(int lane_id) {
    if (lane_id < 0 || lane_id >= LANE_COUNT) return;
    lanes[lane_id].active  = false;
    lanes[lane_id].task_fn = NULL;
    lanes[lane_id].context = NULL;
}

void lane_tick(void) {
    for (int i = 0; i < LANE_COUNT; i++) {
        if (lanes[i].active && lanes[i].task_fn) {
            lanes[i].task_fn(lanes[i].context);
            lanes[i].ticks++;
        }
    }
}

int lane_count_active(void) {
    int count = 0;
    for (int i = 0; i < LANE_COUNT; i++) {
        if (lanes[i].active) count++;
    }
    return count;
}

Lane *lane_get(int lane_id) {
    if (lane_id < 0 || lane_id >= LANE_COUNT) return NULL;
    return &lanes[lane_id];
}
