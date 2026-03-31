#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

#define MAX_BH_SUMMARIES 256

static BHSummary bh_summaries[MAX_BH_SUMMARIES];
static int       bh_summary_count = 0;

void bh_init(void) {
    memset(bh_summaries, 0, sizeof(bh_summaries));
    bh_summary_count = 0;
}

int bh_compress_history(uint64_t from_time, uint64_t to_time) {
    if (bh_summary_count >= MAX_BH_SUMMARIES) return -1;

    BHSummary *s = &bh_summaries[bh_summary_count];
    s->summary_id   = bh_summary_count;
    s->from_time    = from_time;
    s->to_time      = to_time;
    s->record_count = 0;
    s->energy_avg   = 0.0f;

    /* Summarize WH records in range */
    int cnt = wh_count();
    float energy_sum = 0.0f;
    int   in_range   = 0;
    for (int i = 0; i < cnt; i++) {
        WHRecord *r = wh_get(i);
        if (r && r->timestamp >= from_time && r->timestamp <= to_time) {
            in_range++;
            /* Use metadata[0] as proxy energy */
            energy_sum += r->metadata[0] / 255.0f;
        }
    }
    s->record_count = in_range;
    s->energy_avg   = in_range > 0 ? energy_sum / in_range : 0.0f;

    return bh_summary_count++;
}

BHSummary *bh_get_summary(int summary_id) {
    if (summary_id < 0 || summary_id >= bh_summary_count) return NULL;
    return &bh_summaries[summary_id];
}

void bh_forget(int record_id) {
    WHRecord *r = wh_get(record_id);
    if (r) {
        memset(r->metadata, 0, sizeof(r->metadata));
        r->data_size = 0;
    }
}

int bh_summarize_pattern(const int *pattern_ids, int count) {
    if (bh_summary_count >= MAX_BH_SUMMARIES) return -1;
    BHSummary *s = &bh_summaries[bh_summary_count];
    s->summary_id   = bh_summary_count;
    s->from_time    = 0;
    s->to_time      = 0;
    s->record_count = count;
    s->energy_avg   = 0.5f;
    (void)pattern_ids;
    return bh_summary_count++;
}
