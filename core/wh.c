#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

#define MAX_WH_RECORDS 1024

static WHRecord wh_records[MAX_WH_RECORDS];
static int      wh_count_val = 0;

void wh_init(void) {
    memset(wh_records, 0, sizeof(wh_records));
    wh_count_val = 0;
}

int wh_record(uint64_t timestamp, const uint8_t *data, size_t size) {
    if (wh_count_val >= MAX_WH_RECORDS) return -1;
    WHRecord *r = &wh_records[wh_count_val];
    r->record_id          = wh_count_val;
    r->timestamp          = timestamp;
    r->canvas_snapshot_id = wh_count_val;
    r->data_size          = size < 64 ? size : 64;
    if (data && size > 0)
        memcpy(r->metadata, data, r->data_size);
    return wh_count_val++;
}

WHRecord *wh_get(int record_id) {
    if (record_id < 0 || record_id >= wh_count_val) return NULL;
    return &wh_records[record_id];
}

int wh_count(void) {
    return wh_count_val;
}

WHRecord *wh_latest(void) {
    if (wh_count_val == 0) return NULL;
    return &wh_records[wh_count_val - 1];
}
