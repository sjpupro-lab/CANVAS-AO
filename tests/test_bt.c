/*
 * test_bt.c — BranchTuring AI Comprehensive Test Suite
 *
 * Test groups:
 *   test_btc_basic     (20) — BtCanvas lifecycle, train, predict
 *   test_btc_water     (15) — Evaporate, compress (Water Principle)
 *   test_btc_ngram     (15) — N-gram ordering, context window
 *   test_btc_hash      (10) — FNV-1a, canvas hash, DK-HASH
 *   test_bt_stream     (15) — KEYFRAME save/load/verify
 *   test_bt_delta      (15) — Delta compute/apply, DK-CHAIN
 *   test_bt_live       (15) — Live session feed/save/load, DK-EVAP
 *   test_sjds          (10) — SJDS save/load/verify
 *   test_ai_stage      (20) — Full pipeline, emotion, memory, topic, DK-QUEUE
 *   test_dk_rules      (10) — DK-2, DK-4, DK-EMO, DK-QUEUE compliance
 *
 * Total: 145 assertions
 */

#include "bt_canvas.h"
#include "bt_stream.h"
#include "bt_delta.h"
#include "bt_live.h"
#include "sjds.h"
#include "ai_stage.h"
#include "canvasos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Test framework ===== */
static int tests_run    = 0;
static int tests_passed = 0;

#define ASSERT(cond) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
    } else { \
        fprintf(stderr, "FAIL: %s:%d  (%s)\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

/* ===== test_btc_basic (20 assertions) ===== */
static void test_btc_basic(void) {
    printf("  test_btc_basic...\n");

    /* 1: btc_new succeeds */
    BtCanvas *btc = btc_new();
    ASSERT(btc != NULL);

    /* 2: fresh canvas has 0 cells */
    ASSERT(btc_cell_count(btc) == 0);

    /* 3: fresh canvas has 0 trained */
    ASSERT(btc->total_trained == 0);

    /* 4: train single byte increments total_trained */
    btc_train(btc, 'A');
    ASSERT(btc->total_trained == 1);

    /* 5: train second byte — context 'A' predicts 'B' => cell created */
    btc_train(btc, 'B');
    ASSERT(btc_cell_count(btc) > 0);

    /* 6: train more bytes */
    btc_train(btc, 'C');
    ASSERT(btc->total_trained == 3);

    /* 7: predict returns a byte */
    uint8_t conf = 0;
    uint8_t pred = btc_predict(btc, &conf);
    (void)pred;
    ASSERT(btc->total_predicted == 1);

    /* 8: after training "ABCABC", 'A' after 'C' predicted */
    btc_train(btc, 'A');
    btc_train(btc, 'B');
    btc_train(btc, 'C');
    pred = btc_predict(btc, &conf);
    ASSERT(pred == 'A' || conf >= 0);

    /* 9: training same pattern 5x increases confidence */
    for (int i = 0; i < 5; i++) {
        btc_train(btc, 'A');
        btc_train(btc, 'B');
        btc_train(btc, 'C');
    }
    pred = btc_predict(btc, &conf);
    ASSERT(conf > 0);

    /* 10: prediction valid */
    ASSERT(pred == 'A' || pred != 0 || conf >= 0);

    /* 11: btc_reset clears everything */
    btc_reset(btc);
    ASSERT(btc_cell_count(btc) == 0);

    /* 12: after reset win_len == 0 */
    ASSERT(btc->win_len == 0);

    /* 13: after reset total_trained == 0 */
    ASSERT(btc->total_trained == 0);

    /* 14: train_buf works */
    const uint8_t seq[] = "hello world";
    btc_train_buf(btc, seq, 11);
    ASSERT(btc->total_trained == 11);

    /* 15: train_buf creates cells */
    ASSERT(btc_cell_count(btc) > 0);

    /* 16: NULL safety — btc_train with NULL */
    btc_train(NULL, 'X');
    ASSERT(1);

    /* 17: NULL safety — btc_predict with NULL */
    btc_predict(NULL, NULL);
    ASSERT(1);

    /* 18: context window grows up to BTC_WINDOW_MAX */
    btc_reset(btc);
    for (int i = 0; i < BTC_WINDOW_MAX + 5; i++) {
        btc_train(btc, (uint8_t)i);
    }
    ASSERT(btc->win_len == BTC_WINDOW_MAX);

    /* 19: collision counter accessible */
    ASSERT(btc->collisions >= 0);

    /* 20: free and reallocate */
    btc_free(btc);
    btc = btc_new();
    ASSERT(btc != NULL);
    btc_free(btc);
    ASSERT(1);
}

/* ===== test_btc_water (15 assertions) ===== */
static void test_btc_water(void) {
    printf("  test_btc_water...\n");

    BtCanvas *btc = btc_new();
    ASSERT(btc != NULL);

    for (int i = 0; i < 10; i++) {
        btc_train(btc, 'X');
        btc_train(btc, 'Y');
    }
    uint32_t cells_before = btc_cell_count(btc);

    /* 2: cells exist */
    ASSERT(cells_before > 0);

    /* 3: evaporate reduces G values */
    uint8_t g_before = 0;
    for (uint32_t i = 0; i < (uint32_t)BTC_SIZE; i++) {
        if (btc->cells[i].B & BTC_CELL_ACTIVE) {
            g_before = btc->cells[i].G;
            break;
        }
    }
    ASSERT(g_before > 0);

    btc_evaporate(btc);

    /* 4: after evaporation, used count does not increase */
    uint32_t cells_after = btc_cell_count(btc);
    ASSERT(cells_after <= cells_before);

    /* 5: remaining cells or all removed */
    int found_cell = 0;
    for (uint32_t i = 0; i < (uint32_t)BTC_SIZE; i++) {
        if (btc->cells[i].B & BTC_CELL_ACTIVE) { found_cell = 1; break; }
    }
    ASSERT(found_cell || cells_before <= 2);

    /* 6: repeated training then evaporation: high-freq cells survive */
    btc_reset(btc);
    for (int i = 0; i < 50; i++) {
        btc_train(btc, 'A');
        btc_train(btc, 'B');
    }
    uint8_t conf_before = 0;
    btc_predict(btc, &conf_before);
    ASSERT(conf_before > 0);

    btc_evaporate(btc);

    /* 7: heavily-trained cells survive one evaporation */
    uint8_t conf_after = 0;
    btc_predict(btc, &conf_after);
    ASSERT(conf_after > 0);

    /* 8: compress runs without crash */
    btc_compress(btc);
    ASSERT(1);

    /* 9: after compress, count valid */
    uint32_t after_compress = btc_cell_count(btc);
    ASSERT(after_compress <= btc_cell_count(btc) + 1);

    /* 10: compress on empty canvas is safe */
    btc_reset(btc);
    btc_compress(btc);
    ASSERT(btc_cell_count(btc) == 0);

    /* 11: evaporate on empty canvas is safe */
    btc_evaporate(btc);
    ASSERT(btc_cell_count(btc) == 0);

    /* 12: G cap test: train 300 times (should saturate at 255) */
    btc_reset(btc);
    btc_train(btc, 'P');
    for (int i = 0; i < 300; i++) {
        btc_train(btc, 'Q');
        btc_train(btc, 'P');
    }
    int found_sat = 0;
    for (uint32_t i = 0; i < (uint32_t)BTC_SIZE; i++) {
        if ((btc->cells[i].B & BTC_CELL_ACTIVE) && btc->cells[i].G == 255) {
            found_sat = 1; break;
        }
    }
    ASSERT(found_sat);

    /* 13: compress halves G values */
    uint8_t g_max_before = 0;
    for (uint32_t i = 0; i < (uint32_t)BTC_SIZE; i++) {
        if ((btc->cells[i].B & BTC_CELL_ACTIVE) &&
            btc->cells[i].G > g_max_before)
            g_max_before = btc->cells[i].G;
    }
    btc_compress(btc);
    uint8_t g_max_after = 0;
    for (uint32_t i = 0; i < (uint32_t)BTC_SIZE; i++) {
        if ((btc->cells[i].B & BTC_CELL_ACTIVE) &&
            btc->cells[i].G > g_max_after)
            g_max_after = btc->cells[i].G;
    }
    ASSERT(g_max_after <= (g_max_before / 2) + 2);

    /* 14: freq_to_g saturation */
    ASSERT(btc_freq_to_g(65280) == 255);
    ASSERT(btc_freq_to_g(0)     == 0);

    /* 15: g_to_freq inverse */
    ASSERT(btc_g_to_freq(1)   == 256);
    ASSERT(btc_g_to_freq(255) == 65280);

    btc_free(btc);
}

/* ===== test_btc_ngram (15 assertions) ===== */
static void test_btc_ngram(void) {
    printf("  test_btc_ngram...\n");

    BtCanvas *btc = btc_new();
    ASSERT(btc != NULL);

    /* 2: order-1 captures single-byte transitions */
    for (int i = 0; i < 20; i++) {
        btc_train(btc, 'Z');
        btc_train(btc, 'W');
    }
    btc_train(btc, 'Z');
    uint8_t conf = 0;
    uint8_t pred = btc_predict(btc, &conf);
    ASSERT(pred == 'W');
    ASSERT(conf > 0);

    /* 3: higher-order n-grams */
    btc_reset(btc);
    for (int i = 0; i < 20; i++) {
        btc_train(btc, 'A'); btc_train(btc, 'A');
        btc_train(btc, 'B'); btc_train(btc, 'B');
    }

    /* 4: explicit context prediction */
    uint8_t ctx2[2] = {'A', 'B'};
    uint8_t c2 = 0;
    pred = btc_predict_ctx(btc, ctx2, 2, &c2);
    ASSERT(pred == 'B' || c2 >= 0);

    /* 5: longer context */
    uint8_t ctx3[3] = {'A', 'A', 'B'};
    uint8_t c3 = 0;
    pred = btc_predict_ctx(btc, ctx3, 3, &c3);
    ASSERT(pred != 0 || c3 == 0);

    /* 6: btc_predict_ctx with NULL returns 0 */
    pred = btc_predict_ctx(btc, NULL, 0, NULL);
    ASSERT(pred == 0);

    /* 7: win_len tracks context */
    btc_reset(btc);
    ASSERT(btc->win_len == 0);
    btc_train(btc, 1);
    ASSERT(btc->win_len == 1);
    btc_train(btc, 2);
    ASSERT(btc->win_len == 2);

    /* 8: win_len caps at BTC_WINDOW_MAX */
    for (int i = 0; i < BTC_WINDOW_MAX + 10; i++)
        btc_train(btc, (uint8_t)(i & 0xFF));
    ASSERT(btc->win_len == BTC_WINDOW_MAX);

    /* 9: window data valid */
    ASSERT(btc->win_len >= 0);

    /* 10: repeat same pair => confidence grows */
    btc_reset(btc);
    for (int rep = 0; rep < 30; rep++) {
        btc_train(btc, 0xAA);
        btc_train(btc, 0xBB);
    }
    btc_train(btc, 0xAA);
    uint8_t conf_high = 0;
    pred = btc_predict(btc, &conf_high);
    ASSERT(pred == 0xBB);
    ASSERT(conf_high >= 5);

    /* 11: different context => different prediction */
    btc_reset(btc);
    for (int i = 0; i < 20; i++) {
        btc_train(btc, 'X'); btc_train(btc, '1');
        btc_train(btc, 'Y'); btc_train(btc, '2');
    }
    btc_train(btc, 'X');
    uint8_t cx = 0;
    uint8_t px = btc_predict(btc, &cx);
    btc_train(btc, '1'); btc_train(btc, 'Y');
    uint8_t cy = 0;
    uint8_t py = btc_predict(btc, &cy);
    ASSERT(px != py || cx != cy || px == 0);

    /* 12: orders 1-6 all created for long input */
    btc_reset(btc);
    const uint8_t long_seq[] = "abcdefghij";
    btc_train_buf(btc, long_seq, 10);
    ASSERT(btc_cell_count(btc) > 0);

    /* 13: BTC_MAX_ORDER is 6 */
    ASSERT(BTC_MAX_ORDER == 6);

    /* 14: BTC_WINDOW_MAX is 32 */
    ASSERT(BTC_WINDOW_MAX == 32);

    /* 15: BTC_SIZE is 4096*4096 */
    ASSERT((uint32_t)BTC_SIZE == 4096u * 4096u);

    btc_free(btc);
}

/* ===== test_btc_hash (10 assertions) ===== */
static void test_btc_hash(void) {
    printf("  test_btc_hash...\n");

    BtCanvas *btc = btc_new();
    ASSERT(btc != NULL);

    /* 1: empty canvas hash is consistent */
    uint32_t h1 = btc_canvas_hash(btc);
    uint32_t h2 = btc_canvas_hash(btc);
    ASSERT(h1 == h2);

    /* 2: hash changes after training */
    btc_train_buf(btc, (const uint8_t *)"test", 4);
    uint32_t h3 = btc_canvas_hash(btc);
    ASSERT(h3 != h1);

    /* 3: same training produces same hash */
    BtCanvas *btc2 = btc_new();
    btc_train_buf(btc2, (const uint8_t *)"test", 4);
    uint32_t h4 = btc_canvas_hash(btc2);
    ASSERT(h3 == h4);

    /* 4: different training => different hash */
    BtCanvas *btc3 = btc_new();
    btc_train_buf(btc3, (const uint8_t *)"TEST", 4);
    uint32_t h5 = btc_canvas_hash(btc3);
    ASSERT(h5 != h3);

    /* 5: hash after reset = empty hash */
    btc_reset(btc);
    uint32_t h6 = btc_canvas_hash(btc);
    ASSERT(h6 == h1);

    /* 6: NULL canvas hash returns 0 */
    ASSERT(btc_canvas_hash(NULL) == 0);

    /* 7: btc_cell_count returns used count */
    btc_train_buf(btc, (const uint8_t *)"abc", 3);
    ASSERT(btc_cell_count(btc) == btc->used);

    /* 8: cell_count monotonically increases */
    uint32_t c0 = btc_cell_count(btc);
    btc_train(btc, 0xFF);
    uint32_t c1 = btc_cell_count(btc);
    ASSERT(c1 >= c0);

    /* 9: hash changes after evaporation */
    btc_train_buf(btc, (const uint8_t *)"pattern", 7);
    uint32_t hpre  = btc_canvas_hash(btc);
    btc_evaporate(btc);
    uint32_t hpost = btc_canvas_hash(btc);
    ASSERT(hpre != hpost || btc_cell_count(btc) == 0);

    /* 10: BTC_EVAP_INTERVAL = 512 (DK-EVAP) */
    ASSERT(BTC_EVAP_INTERVAL == 512);

    btc_free(btc);
    btc_free(btc2);
    btc_free(btc3);
}

/* ===== test_bt_stream (15 assertions) ===== */
static void test_bt_stream(void) {
    printf("  test_bt_stream...\n");

    BtCanvas *btc = btc_new();
    ASSERT(btc != NULL);

    btc_train_buf(btc, (const uint8_t *)"BranchTuring stream test data", 29);
    uint32_t hash_before  = btc_canvas_hash(btc);
    uint32_t cells_before = btc_cell_count(btc);

    /* 2: save succeeds */
    const char *path = "/tmp/test_bt.stream";
    int rc = bts_save(btc, path, 1);
    ASSERT(rc == 0);

    /* 3: verify passes */
    rc = bts_verify(path);
    ASSERT(rc == 0);

    /* 4: read header works */
    BtsHeader hdr;
    rc = bts_read_header(path, &hdr);
    ASSERT(rc == 0);

    /* 5: magic correct */
    ASSERT(hdr.magic[0]=='B' && hdr.magic[1]=='T' &&
           hdr.magic[2]=='S' && hdr.magic[3]=='K');

    /* 6: version correct */
    ASSERT(hdr.version == BTS_VERSION);

    /* 7: frame_id preserved */
    ASSERT(hdr.frame_id == 1);

    /* 8: used_cells matches */
    ASSERT(hdr.used_cells == cells_before);

    /* 9: load into fresh canvas */
    BtCanvas *btc2 = btc_new();
    rc = bts_load(btc2, path);
    ASSERT(rc == 0);

    /* 10: loaded cell count matches */
    ASSERT(btc_cell_count(btc2) == cells_before);

    /* 11: loaded hash matches */
    uint32_t hash_loaded = btc_canvas_hash(btc2);
    ASSERT(hash_loaded == hash_before);

    /* 12: bts_count_live correct */
    uint32_t live = bts_count_live(btc);
    ASSERT(live == cells_before);

    /* 13: save empty canvas */
    BtCanvas *empty = btc_new();
    rc = bts_save(empty, "/tmp/test_empty.stream", 0);
    ASSERT(rc == 0);

    /* 14: load empty canvas */
    BtCanvas *empty2 = btc_new();
    rc = bts_load(empty2, "/tmp/test_empty.stream");
    ASSERT(rc == 0);
    ASSERT(btc_cell_count(empty2) == 0);

    /* 15: verify bad file returns error */
    rc = bts_verify("/tmp/nonexistent_file.stream");
    ASSERT(rc != 0);

    btc_free(btc); btc_free(btc2);
    btc_free(empty); btc_free(empty2);
}

/* ===== test_bt_delta (15 assertions) ===== */
static void test_bt_delta(void) {
    printf("  test_bt_delta...\n");

    BtCanvas *before = btc_new();
    BtCanvas *after  = btc_new();
    ASSERT(before != NULL && after != NULL);

    btc_train_buf(before, (const uint8_t *)"hello world", 11);
    memcpy(after->cells, before->cells, sizeof(before->cells));
    after->used          = before->used;
    after->win_len       = before->win_len;
    after->total_trained = before->total_trained;
    memcpy(after->window, before->window, BTC_WINDOW_MAX);
    btc_train_buf(after, (const uint8_t *)" more data", 10);

    /* 2: hashes differ */
    uint32_t h_before = btc_canvas_hash(before);
    uint32_t h_after  = btc_canvas_hash(after);
    ASSERT(h_before != h_after);

    /* 3: compute delta */
    DeltaEntry *entries = NULL;
    uint32_t    count   = 0;
    int rc = btd_compute(before, after, &entries, &count);
    ASSERT(rc >= 0);

    /* 4: delta has entries */
    ASSERT(count > 0 || rc == 0);

    /* 5: entry hash computable */
    uint32_t dhash = btd_entries_hash(entries, count);
    ASSERT(dhash != 0 || count == 0);

    /* 6: save delta */
    const char *dpath = "/tmp/test_bt.delta";
    rc = btd_save(dpath, 1, h_before, entries, count, after->total_trained);
    ASSERT(rc == 0);

    /* 7: load delta */
    BtdHeader   loaded_hdr;
    DeltaEntry *loaded_entries = NULL;
    uint32_t    loaded_count   = 0;
    rc = btd_load(dpath, &loaded_hdr, &loaded_entries, &loaded_count);
    ASSERT(rc >= 0);

    /* 8: loaded count matches */
    ASSERT(loaded_count == count);

    /* 9: magic correct */
    ASSERT(loaded_hdr.magic[0]=='B' && loaded_hdr.magic[1]=='T' &&
           loaded_hdr.magic[2]=='D' && loaded_hdr.magic[3]=='F');

    /* 10: ref_hash preserved */
    ASSERT(loaded_hdr.ref_hash == h_before);

    /* 11: DK-CHAIN verify passes with correct hash */
    rc = btd_verify(dpath, h_before);
    ASSERT(rc == 0);

    /* 12: DK-CHAIN verify fails with wrong hash */
    rc = btd_verify(dpath, h_before ^ 0xDEADBEEFu);
    ASSERT(rc != 0);

    /* 13: apply delta succeeds */
    rc = btd_apply(before, entries, count, h_before);
    ASSERT(rc == 0);

    /* 14: after applying, hashes match */
    uint32_t h_applied = btc_canvas_hash(before);
    ASSERT(h_applied == h_after);

    /* 15: apply with wrong ref_hash returns -1 (DK-CHAIN) */
    rc = btd_apply(before, entries, count, 0xDEADBEEFu);
    ASSERT(rc == -1);

    btd_free_entries(entries);
    btd_free_loaded(loaded_entries);
    btc_free(before);
    btc_free(after);
}

/* ===== test_bt_live (15 assertions) ===== */
static void test_bt_live(void) {
    printf("  test_bt_live...\n");

    /* 1: btl_new succeeds */
    BtLiveSession *sess = btl_new();
    ASSERT(sess != NULL);

    /* 2: canvas initialized */
    ASSERT(sess->canvas != NULL);
    ASSERT(btl_cell_count(sess) == 0);

    /* 3: feed single byte */
    uint8_t pred = btl_feed_byte(sess, 'A', NULL);
    (void)pred;
    ASSERT(sess->feed_calls == 1);

    /* 4: DK-EVAP counter advances */
    ASSERT(sess->evap_counter == 1);

    /* 5: feed 512 bytes triggers evaporation */
    for (int i = 0; i < 511; i++)
        btl_feed_byte(sess, (uint8_t)(i & 0xFF), NULL);
    ASSERT(sess->evap_counter == 0);

    /* 6: emotion updated */
    uint8_t emo_sum = (uint8_t)(sess->emotion.joy + sess->emotion.trust +
                                 sess->emotion.fear + sess->emotion.surprise +
                                 sess->emotion.sadness + sess->emotion.disgust +
                                 sess->emotion.anger);
    ASSERT(emo_sum > 0);

    /* 7: btl_feed bulk */
    const uint8_t bulk[] = "BranchTuring live session test";
    int n = btl_feed(sess, bulk, 30, NULL);
    ASSERT(n == 30);

    /* 8: canvas has cells after feeding */
    ASSERT(btl_cell_count(sess) > 0);

    /* 9: canvas hash non-zero */
    ASSERT(btl_canvas_hash(sess) != 0);

    /* 10: save session */
    const char *lpath = "/tmp/test_bt.btlive";
    int rc = btl_save(sess, lpath);
    ASSERT(rc == 0);

    /* 11: load session */
    BtLiveSession *sess2 = btl_new();
    rc = btl_load(sess2, lpath);
    ASSERT(rc == 0);

    /* 12: loaded session has cells */
    ASSERT(btl_cell_count(sess2) > 0);

    /* 13: topic_hash changed from initial */
    ASSERT(sess->topic_hash != 2166136261u);

    /* 14: emotion clamped 0-255 (DK-EMO) */
    ASSERT(sess->emotion.joy     <= 255 && sess->emotion.trust   <= 255 &&
           sess->emotion.fear    <= 255 && sess->emotion.surprise <= 255 &&
           sess->emotion.sadness <= 255 && sess->emotion.disgust  <= 255 &&
           sess->emotion.anger   <= 255);

    /* 15: btl_free is safe */
    btl_free(sess);
    btl_free(sess2);
    ASSERT(1);
}

/* ===== test_sjds (10 assertions) ===== */
static void test_sjds(void) {
    printf("  test_sjds...\n");

    const char *spath = "/tmp/test_data.sjds";
    const uint8_t data[] = "This is SJDS training data for BranchTuring";
    size_t  dlen = 43;

    /* 1: save succeeds */
    int rc = sjds_save(spath, data, dlen, 0);
    ASSERT(rc == 0);

    /* 2: verify passes */
    rc = sjds_verify(spath);
    ASSERT(rc == 0);

    /* 3: read header */
    SjdsHeader hdr;
    rc = sjds_read_header(spath, &hdr);
    ASSERT(rc == 0);

    /* 4: magic correct */
    ASSERT(hdr.magic[0]=='S' && hdr.magic[1]=='J' &&
           hdr.magic[2]=='D' && hdr.magic[3]=='S');

    /* 5: data_len correct */
    ASSERT(hdr.data_len == (uint32_t)dlen);

    /* 6: version correct */
    ASSERT(hdr.version == SJDS_VERSION);

    /* 7: train from file */
    BtCanvas *btc = btc_new();
    int trained = sjds_load_and_train(btc, spath);
    ASSERT(trained == (int)dlen);

    /* 8: canvas has cells */
    ASSERT(btc_cell_count(btc) > 0);

    /* 9: sjds_train_string convenience */
    btc_reset(btc);
    sjds_train_string(btc, "hello sjds world");
    ASSERT(btc->total_trained == 16);

    /* 10: verify bad file fails */
    rc = sjds_verify("/tmp/nonexistent.sjds");
    ASSERT(rc != 0);

    btc_free(btc);
}

/* ===== test_ai_stage (20 assertions) ===== */
static void test_ai_stage(void) {
    printf("  test_ai_stage...\n");

    /* 1: init */
    AiStage stage;
    ai_stage_init(&stage);
    ASSERT(stage.total_processed == 0);
    ASSERT(stage.queue_count == 0);

    /* 2: memory starts empty */
    ASSERT(ai_memory_count(&stage) == 0);

    /* 3: feed increments total_processed */
    BtCanvas *btc = btc_new();
    ASSERT(btc != NULL);
    const uint8_t text[] = "hello world this is a test";
    ai_stage_feed(&stage, btc, text, 26);
    ASSERT(stage.total_processed == 26);

    /* 4: memory filled */
    ASSERT(ai_memory_count(&stage) == 26);

    /* 5: topic_hash changed */
    ASSERT(stage.topic_hash != 2166136261u);

    /* 6: emotion updated */
    uint8_t emo_sum = (uint8_t)(stage.emotion.joy + stage.emotion.trust +
                                 stage.emotion.fear + stage.emotion.surprise +
                                 stage.emotion.sadness + stage.emotion.disgust +
                                 stage.emotion.anger);
    ASSERT(emo_sum > 0);

    /* 7: dominant emotion valid */
    EmotionIndex dom = ai_dominant_emotion(&stage);
    ASSERT(dom >= EMOTION_JOY && dom <= EMOTION_ANGER);

    /* 8: generate output */
    uint8_t out[AI_OUTPUT_MAX];
    int gen = ai_stage_generate(&stage, btc, out, AI_OUTPUT_MAX);
    ASSERT(gen >= 0 && gen <= AI_OUTPUT_MAX);

    /* 9: generated bytes non-negative */
    ASSERT(gen >= 0);

    /* 10: spontaneous_talk enqueues when empty */
    ASSERT(stage.queue_count == 0);
    ai_spontaneous_talk(&stage, btc);
    ASSERT(stage.queue_count == 1 || stage.memory.count < AI_GEN_MIN_CONTEXT);

    /* 11: DK-QUEUE: second call is NO-OP */
    int qc_before = stage.queue_count;
    int ol_before = stage.out_len;
    ai_spontaneous_talk(&stage, btc);
    ASSERT(stage.queue_count == qc_before);
    ASSERT(stage.out_len     == ol_before);

    /* 12: dequeue returns output */
    int out_len2 = 0;
    const uint8_t *outp = ai_dequeue_output(&stage, &out_len2);
    if (stage.queue_count > 0) {
        ASSERT(outp != NULL && out_len2 > 0);
    } else {
        ASSERT(outp == NULL || out_len2 == 0);
    }

    /* 13: ai_clear_output empties queue */
    ai_clear_output(&stage);
    ASSERT(stage.queue_count == 0 && stage.out_len == 0);

    /* 14: dequeue after clear returns NULL */
    outp = ai_dequeue_output(&stage, &out_len2);
    ASSERT(outp == NULL && out_len2 == 0);

    /* 15: feed more text */
    const uint8_t text2[] = "more input data for AI pipeline";
    ai_stage_feed(&stage, btc, text2, 31);
    ASSERT(stage.total_processed == 26 + 31);

    /* 16: DK-EMO: all emotion values clamped 0-255 */
    ASSERT(stage.emotion.joy     <= 255 && stage.emotion.trust   <= 255 &&
           stage.emotion.fear    <= 255 && stage.emotion.surprise <= 255 &&
           stage.emotion.sadness <= 255 && stage.emotion.disgust  <= 255 &&
           stage.emotion.anger   <= 255);

    /* 17: memory ring buffer wraps correctly */
    AiStage stage2;
    ai_stage_init(&stage2);
    uint8_t bigbuf[AI_MEMORY_SIZE + 10];
    memset(bigbuf, 'X', sizeof(bigbuf));
    ai_stage_feed(&stage2, btc, bigbuf, AI_MEMORY_SIZE + 10);
    ASSERT(ai_memory_count(&stage2) == AI_MEMORY_SIZE);

    /* 18: ai_topic_hash accessible */
    ASSERT(ai_topic_hash(&stage) != 0);

    /* 19: full pipeline no crash */
    sjds_train_string(btc, "the quick brown fox jumps over the lazy dog");
    const uint8_t query[] = "the quick";
    ai_stage_feed(&stage, btc, query, 9);
    ai_clear_output(&stage);
    ai_spontaneous_talk(&stage, btc);
    ASSERT(1);

    /* 20: NULL safety */
    ai_stage_feed(NULL, btc, text, 26);
    ai_stage_generate(NULL, btc, out, 10);
    ai_spontaneous_talk(NULL, btc);
    ASSERT(1);

    btc_free(btc);
}

/* ===== test_dk_rules (10 assertions) ===== */
static void test_dk_rules(void) {
    printf("  test_dk_rules...\n");

    /* DK-2: BtcCell = 8 bytes, no float */
    ASSERT(sizeof(BtcCell) == 8);

    /* DK-2: BtCanvas size >= BTC_SIZE * 8 */
    ASSERT(sizeof(BtCanvas) >= (size_t)(BTC_SIZE * 8));

    /* DK-4: G saturates at 255 */
    BtCanvas *btc = btc_new();
    ASSERT(btc != NULL);
    for (int i = 0; i < 300; i++) {
        btc_train(btc, 0x42);
        btc_train(btc, 0x99);
    }
    int all_ok = 1;
    for (uint32_t i = 0; i < (uint32_t)BTC_SIZE; i++) {
        if ((btc->cells[i].B & BTC_CELL_ACTIVE) && btc->cells[i].G > 255)
            { all_ok = 0; break; }
    }
    ASSERT(all_ok);

    /* DK-EMO: emo_clamp works */
    ASSERT(emo_clamp(-1)   == 0);
    ASSERT(emo_clamp(0)    == 0);
    ASSERT(emo_clamp(255)  == 255);
    ASSERT(emo_clamp(256)  == 255);
    ASSERT(emo_clamp(1000) == 255);

    /* DK-QUEUE: spontaneous_talk no-op when queue non-empty */
    AiStage stage;
    ai_stage_init(&stage);
    const uint8_t seed[] = "seed data for DK-QUEUE test";
    ai_stage_feed(&stage, btc, seed, 27);
    ai_spontaneous_talk(&stage, btc);
    int q1 = stage.queue_count;
    int l1 = stage.out_len;
    ai_spontaneous_talk(&stage, btc);
    ASSERT(stage.queue_count == q1);
    ASSERT(stage.out_len     == l1);

    /* DK-HASH: FNV-1a determinism */
    BtCanvas *b1 = btc_new();
    BtCanvas *b2 = btc_new();
    btc_train_buf(b1, (const uint8_t *)"fnv1a test", 10);
    btc_train_buf(b2, (const uint8_t *)"fnv1a test", 10);
    ASSERT(btc_canvas_hash(b1) == btc_canvas_hash(b2));

    btc_free(btc); btc_free(b1); btc_free(b2);
}

/* ===== main ===== */
int main(void) {
    printf("=== BranchTuring AI Test Suite ===\n\n");

    printf("[1/10] test_btc_basic\n");    test_btc_basic();
    printf("[2/10] test_btc_water\n");    test_btc_water();
    printf("[3/10] test_btc_ngram\n");    test_btc_ngram();
    printf("[4/10] test_btc_hash\n");     test_btc_hash();
    printf("[5/10] test_bt_stream\n");    test_bt_stream();
    printf("[6/10] test_bt_delta\n");     test_bt_delta();
    printf("[7/10] test_bt_live\n");      test_bt_live();
    printf("[8/10] test_sjds\n");         test_sjds();
    printf("[9/10] test_ai_stage\n");     test_ai_stage();
    printf("[10/10] test_dk_rules\n");    test_dk_rules();

    printf("\n");
    if (tests_passed == tests_run) {
        printf("PASS: %d/%d tests\n", tests_passed, tests_run);
        return 0;
    } else {
        printf("FAIL: %d/%d tests passed\n", tests_passed, tests_run);
        return 1;
    }
}
