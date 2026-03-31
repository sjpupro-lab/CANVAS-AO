#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_FRAMES 4096

static Frame  frame_store[MAX_FRAMES];
static int    frame_count = 0;
static uint64_t tick_counter = 0;

static uint32_t checksum32(const uint8_t *data, size_t len) {
    uint32_t crc = 0;
    for (size_t i = 0; i < len; i++) crc = crc * 31 + data[i];
    return crc;
}

static uint64_t now_ts(void) {
    return (uint64_t)time(NULL) * 1000 + (tick_counter++);
}

int stream_write_keyframe(const uint8_t *data, size_t size) {
    if (frame_count >= MAX_FRAMES) return -1;
    size_t sz = size > 1024 ? 1024 : size;
    Frame *f = &frame_store[frame_count];
    f->type      = FRAME_KEY;
    f->timestamp = now_ts();
    memcpy(f->data, data, sz);
    f->data_size = sz;
    f->checksum  = checksum32(data, sz);
    f->prev_id   = -1;
    return frame_count++;
}

int stream_write_delta(int prev_id, const uint8_t *data, size_t size) {
    if (frame_count >= MAX_FRAMES) return -1;
    if (prev_id < 0 || prev_id >= frame_count) return -1;
    size_t sz = size > 1024 ? 1024 : size;
    Frame *f = &frame_store[frame_count];
    f->type      = FRAME_DELTA;
    f->timestamp = now_ts();
    memcpy(f->data, data, sz);
    f->data_size = sz;
    f->checksum  = checksum32(data, sz);
    f->prev_id   = prev_id;
    return frame_count++;
}

Frame *stream_read(int frame_id) {
    if (frame_id < 0 || frame_id >= frame_count) return NULL;
    return &frame_store[frame_id];
}

int stream_reconstruct(int frame_id, uint8_t *out, size_t out_size) {
    if (frame_id < 0 || frame_id >= frame_count) return -1;

    /* Walk back to find keyframe */
    int chain[MAX_FRAMES];
    int chain_len = 0;
    int cur = frame_id;
    while (cur >= 0) {
        chain[chain_len++] = cur;
        if (frame_store[cur].type == FRAME_KEY) break;
        cur = frame_store[cur].prev_id;
    }

    /* Apply keyframe first, then deltas */
    size_t   out_sz = 0;
    uint8_t  base[1024];
    memset(base, 0, sizeof(base));

    for (int i = chain_len - 1; i >= 0; i--) {
        Frame *f = &frame_store[chain[i]];
        if (f->type == FRAME_KEY) {
            memcpy(base, f->data, f->data_size);
            out_sz = f->data_size;
        } else {
            /* XOR delta application */
            size_t apply = f->data_size < out_sz ? f->data_size : out_sz;
            for (size_t j = 0; j < apply; j++) base[j] ^= f->data[j];
        }
    }

    size_t copy = out_sz < out_size ? out_sz : out_size;
    memcpy(out, base, copy);
    return (int)copy;
}

int stream_frame_count(void) {
    return frame_count;
}

void stream_reset(void) {
    frame_count  = 0;
    tick_counter = 0;
    memset(frame_store, 0, sizeof(frame_store));
}
