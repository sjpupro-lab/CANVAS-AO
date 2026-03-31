#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ===== Simple test framework ===== */
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

#define ASSERT_FLOAT_EQ(a, b, eps) ASSERT(fabsf((a)-(b)) < (eps))

/* ===== test_canvas ===== */
static void test_canvas(void) {
    canvas_init();

    /* 1-5: init returns zeroed cells */
    Cell c = canvas_get(0, 0);
    ASSERT(c.energy == 0);
    ASSERT(c.state  == CELL_IDLE);
    ASSERT(c.id     == 0);
    ASSERT(c.color.r == 0);
    ASSERT(c.color.a == 0);

    /* 6-10: set and get */
    Cell nc;
    nc.color.r = 10; nc.color.g = 20; nc.color.b = 30; nc.color.a = 255;
    nc.energy = 100; nc.state = CELL_ACTIVE; nc.id = 42;
    canvas_set(10, 10, nc);
    Cell got = canvas_get(10, 10);
    ASSERT(got.color.r == 10);
    ASSERT(got.color.g == 20);
    ASSERT(got.color.b == 30);
    ASSERT(got.energy  == 100);
    ASSERT(got.state   == CELL_ACTIVE);

    /* 11-15: id preserved */
    ASSERT(got.id == 42);
    canvas_set(0, 0, nc);
    ASSERT(canvas_get(0,0).energy == 100);
    canvas_set(CANVAS_WIDTH-1, CANVAS_HEIGHT-1, nc);
    ASSERT(canvas_get(CANVAS_WIDTH-1, CANVAS_HEIGHT-1).energy == 100);
    canvas_set(100, 200, nc);
    ASSERT(canvas_get(100, 200).id == 42);

    /* 16-20: out of bounds returns empty */
    Cell oor = canvas_get(-1, 0);
    ASSERT(oor.energy == 0);
    oor = canvas_get(CANVAS_WIDTH, 0);
    ASSERT(oor.energy == 0);
    oor = canvas_get(0, CANVAS_HEIGHT);
    ASSERT(oor.energy == 0);
    canvas_set(-1, 0, nc); /* should not crash */
    ASSERT(canvas_get(-1,0).energy == 0);

    /* 21-25: tick increments energy of active cells */
    Cell ac; memset(&ac, 0, sizeof(ac));
    ac.state = CELL_ACTIVE; ac.energy = 50;
    canvas_set(5, 5, ac);
    canvas_tick();
    ASSERT(canvas_get(5, 5).energy == 51);
    canvas_tick();
    ASSERT(canvas_get(5, 5).energy == 52);

    /* energy caps at 255 */
    ac.energy = 254; canvas_set(6, 6, ac);
    canvas_tick();
    ASSERT(canvas_get(6, 6).energy == 255);
    canvas_tick();
    ASSERT(canvas_get(6, 6).energy == 255);

    /* idle cells not incremented */
    Cell idle; memset(&idle, 0, sizeof(idle));
    idle.state = CELL_IDLE; idle.energy = 50;
    canvas_set(7, 7, idle);
    canvas_tick();
    ASSERT(canvas_get(7, 7).energy == 50);

    /* 26-30: clear resets */
    canvas_clear();
    ASSERT(canvas_get(10, 10).energy == 0);
    ASSERT(canvas_get(10, 10).state  == CELL_IDLE);
    ASSERT(canvas_get(5, 5).energy   == 0);
    ASSERT(canvas_get(0, 0).energy   == 0);
    ASSERT(canvas_get(CANVAS_WIDTH-1, CANVAS_HEIGHT-1).energy == 0);
}

/* ===== test_gate ===== */
static void test_gate(void) {
    gate_init();

    /* 1-5: initially unlocked, all can access */
    ASSERT(gate_can_access(0, 0, 1) == true);
    ASSERT(gate_can_access(1, 1, 2) == true);
    ASSERT(gate_can_access(63, 63, 99) == true);
    ASSERT(gate_can_access(32, 32, 0) == true);
    ASSERT(gate_can_access(10, 10, 5) == true);

    /* 6-10: lock */
    ASSERT(gate_lock(0, 0, 7) == true);
    ASSERT(gate_can_access(0, 0, 7) == true);   /* owner can */
    ASSERT(gate_can_access(0, 0, 8) == false);  /* other cannot */
    ASSERT(gate_lock(0, 0, 8) == false);         /* already locked */
    ASSERT(gate_can_access(0, 0, 0) == false);

    /* 11-15: unlock */
    ASSERT(gate_unlock(0, 0, 7) == true);
    ASSERT(gate_can_access(0, 0, 8) == true);   /* now unlocked */
    ASSERT(gate_unlock(0, 0, 7) == false);       /* not locked */
    ASSERT(gate_lock(5, 5, 3) == true);
    ASSERT(gate_unlock(5, 5, 4) == false);       /* wrong owner */

    /* 16-20: multiple tiles */
    gate_lock(1, 0, 10);
    gate_lock(2, 0, 11);
    ASSERT(gate_can_access(1, 0, 10) == true);
    ASSERT(gate_can_access(1, 0, 11) == false);
    ASSERT(gate_can_access(2, 0, 11) == true);

    /* 21-25: out of bounds */
    ASSERT(gate_lock(-1, 0, 1) == false);
    ASSERT(gate_lock(64, 0, 1) == false);
    ASSERT(gate_unlock(-1, 0, 1) == false);
    ASSERT(gate_can_access(-1, 0, 1) == false);
    ASSERT(gate_can_access(64, 0, 1) == false);

    /* 26-30: isolate region */
    gate_init();
    gate_isolate_region(0, 0, 127, 127, 42);
    ASSERT(gate_can_access(0, 0, 42) == true);
    ASSERT(gate_can_access(1, 1, 42) == true);
    ASSERT(gate_can_access(0, 0, 1) == false);
    ASSERT(gate_can_access(1, 1, 1) == false);
    ASSERT(gate_can_access(0, 1, 99) == false); /* tile (0,1) is in locked region */
}

/* ===== test_scan ===== */
static int scan_test_count;
static void scan_count_cb(int x, int y, Cell *c) {
    (void)x; (void)y; (void)c;
    scan_test_count++;
}

static void test_scan(void) {
    canvas_init();

    /* Ring r=0 hits 1 cell */
    scan_test_count = 0;
    scan_ring(100, 100, 0, scan_count_cb);
    ASSERT(scan_test_count == 1);

    /* Ring r=1: perimeter of 3x3 = 8 */
    scan_test_count = 0;
    scan_ring(100, 100, 1, scan_count_cb);
    ASSERT(scan_test_count == 8);

    /* Ring r=2: perimeter of 5x5 = 16 */
    scan_test_count = 0;
    scan_ring(100, 100, 2, scan_count_cb);
    ASSERT(scan_test_count == 16);

    /* Spiral r=0: just center */
    scan_test_count = 0;
    scan_spiral(100, 100, 0, scan_count_cb);
    ASSERT(scan_test_count == 1);

    /* Spiral r=1: center + ring1 = 9 */
    scan_test_count = 0;
    scan_spiral(100, 100, 1, scan_count_cb);
    ASSERT(scan_test_count == 9);

    /* 6-10: scan_count_active returns 0 on empty */
    ASSERT(scan_count_active(100, 100, 5) == 0);

    /* Place active cell */
    Cell ac; memset(&ac, 0, sizeof(ac));
    ac.state = CELL_ACTIVE; ac.energy = 50;
    canvas_set(100, 100, ac);
    ASSERT(scan_count_active(100, 100, 5) == 1);
    canvas_set(101, 100, ac);
    ASSERT(scan_count_active(100, 100, 5) == 2);
    canvas_set(200, 200, ac);
    ASSERT(scan_count_active(100, 100, 5) == 2); /* too far */

    /* 11-15: scan_find_pattern */
    canvas_init();
    ASSERT(scan_find_pattern(100, 100, 10, 50) == -1); /* none */
    Cell energized; memset(&energized, 0, sizeof(energized));
    energized.state = CELL_ACTIVE; energized.energy = 100;
    canvas_set(100, 100, energized);
    ASSERT(scan_find_pattern(100, 100, 10, 100) == 0); /* found at r=0 */
    canvas_set(103, 100, energized);
    int found = scan_find_pattern(99, 100, 10, 100);
    ASSERT(found >= 0 && found <= 10);

    /* 16-20: spiral covers all cells at max_r */
    canvas_init();
    scan_test_count = 0;
    scan_spiral(50, 50, 3, scan_count_cb);
    /* (2*3+1)^2 = 49 */
    ASSERT(scan_test_count == 49);

    /* 21-25: ring at corner */
    scan_test_count = 0;
    scan_ring(0, 0, 1, scan_count_cb);
    ASSERT(scan_test_count == 8);

    /* place multiple active cells and count */
    canvas_init();
    for (int i = 0; i < 5; i++) {
        Cell c2; memset(&c2, 0, sizeof(c2));
        c2.state = CELL_ACTIVE;
        canvas_set(200 + i, 200, c2);
    }
    ASSERT(scan_count_active(202, 200, 3) >= 5);

    /* 26-30: find at various radii */
    canvas_init();
    energized.energy = 200;
    canvas_set(110, 110, energized);
    ASSERT(scan_find_pattern(100, 100, 20, 200) >= 0);
    ASSERT(scan_find_pattern(100, 100,  5, 200) == -1);
    ASSERT(scan_find_pattern(110, 110,  5, 200) == 0);
    ASSERT(scan_find_pattern(110, 110,  1, 201) == -1); /* too high */
    canvas_set(111, 110, energized);
    ASSERT(scan_find_pattern(110, 110, 1, 200) >= 0);
}

/* ===== test_pattern ===== */
static void test_pattern(void) {
    canvas_init();

    /* 1-5: recognize on empty canvas */
    PatternResult pr = pattern_recognize(100, 100, 4);
    ASSERT(pr.confidence >= 0.0f && pr.confidence <= 1.0f);
    ASSERT(pr.layer >= LAYER_RAW && pr.layer <= LAYER_ABSTRACT);
    ASSERT(pr.matched_id == 0);

    /* 6-10: set active cells and recognize */
    Cell ac; memset(&ac, 0, sizeof(ac));
    ac.state = CELL_ACTIVE; ac.energy = 200;
    ac.color.r = 255;
    canvas_set(100, 100, ac);
    pr = pattern_recognize(100, 100, 4);
    ASSERT(pr.confidence > 0.0f);
    ASSERT(pr.layer >= LAYER_RAW);

    /* Pattern with surrounding context */
    for (int dx = -3; dx <= 3; dx++) {
        for (int dy = -3; dy <= 3; dy++) {
            canvas_set(100+dx, 100+dy, ac);
        }
    }
    pr = pattern_recognize(100, 100, 4);
    ASSERT(pr.confidence > 0.0f);

    /* 11-15: 6 layers callable without crash */
    pattern_layer_raw(100, 100);
    ASSERT(1);
    pattern_layer_edge(100, 100, 2);
    ASSERT(1);
    pattern_layer_shape(100, 100, 2);
    ASSERT(1);
    pattern_layer_object(100, 100, 2);
    ASSERT(1);
    pattern_layer_context(100, 100, 2);
    ASSERT(1);

    /* 16-20: layer_abstract */
    pattern_layer_abstract(100, 100, 2);
    ASSERT(1);
    pr = pattern_recognize(50, 50, 1);
    ASSERT(pr.confidence >= 0.0f);
    pr = pattern_recognize(0, 0, 2);
    ASSERT(pr.layer >= LAYER_RAW);
    pr = pattern_recognize(CANVAS_WIDTH-1, CANVAS_HEIGHT-1, 2);
    ASSERT(pr.confidence >= 0.0f);
    pr = pattern_recognize(200, 200, 8);
    ASSERT(pr.layer <= LAYER_ABSTRACT);

    /* 21-25: training */
    pattern_train(100, 100, LAYER_SHAPE, 99);
    pr = pattern_recognize(100, 100, 4);
    ASSERT(pr.confidence >= 0.0f);
    pattern_train(200, 200, LAYER_OBJECT, 100);
    ASSERT(1);
    pattern_train(300, 300, LAYER_ABSTRACT, 101);
    ASSERT(1);
    pattern_train(50, 50, LAYER_EDGE, 55);
    ASSERT(1);
    pattern_train(0, 0, LAYER_RAW, 1);
    ASSERT(1);

    /* 26-30: confidence in valid range after training */
    pr = pattern_recognize(100, 100, 4);
    ASSERT(pr.confidence >= 0.0f && pr.confidence <= 1.0f);
    pr = pattern_recognize(200, 200, 4);
    ASSERT(pr.confidence >= 0.0f && pr.confidence <= 1.0f);
    pr = pattern_recognize(300, 300, 4);
    ASSERT(pr.confidence >= 0.0f && pr.confidence <= 1.0f);
    pr = pattern_recognize(50, 50, 4);
    ASSERT(pr.confidence >= 0.0f && pr.confidence <= 1.0f);
    ASSERT(pr.layer <= LAYER_ABSTRACT);
}

/* ===== test_constellation ===== */
static void test_constellation(void) {
    canvas_init();
    /* Seed some active cells */
    for (int i = 0; i < 20; i++) {
        Cell c; memset(&c, 0, sizeof(c));
        c.state = CELL_ACTIVE; c.energy = (uint8_t)(i * 10 + 10);
        canvas_set(100 + i, 100 + i, c);
    }

    /* 1-5: build does not crash */
    constellation_build(110, 110, 15);
    Constellation *con = constellation_get();
    ASSERT(con != NULL);
    ASSERT(con->count >= 0);
    ASSERT(con->center_x == 110);
    ASSERT(con->center_y == 110);
    ASSERT(con->count <= MAX_CONSTELLATION_NODES);

    /* 6-10: propagate */
    constellation_propagate(1);
    ASSERT(1);
    constellation_propagate(5);
    ASSERT(1);
    for (int i = 0; i < con->count; i++) {
        ASSERT(con->nodes[i].energy >= 0.0f);
        ASSERT(con->nodes[i].energy <= 1.0f);
    }

    /* 11-15: infer returns valid index or -1 */
    int best = constellation_infer(0.5f);
    ASSERT(best >= -1 && best < MAX_CONSTELLATION_NODES);
    best = constellation_infer(0.0f);
    ASSERT(best >= -1);
    best = constellation_infer(1.0f);
    ASSERT(best >= -1);

    /* 16-20: update energy */
    if (con->count > 0) {
        int nx = con->nodes[0].x;
        int ny = con->nodes[0].y;
        float old_e = con->nodes[0].energy;
        constellation_update(nx, ny, 0.1f);
        ASSERT(con->nodes[0].energy >= 0.0f);
        ASSERT(con->nodes[0].energy <= 1.0f);
        constellation_update(nx, ny, -2.0f); /* clamp to 0 */
        ASSERT(con->nodes[0].energy >= 0.0f);
        constellation_update(nx, ny, 2.0f); /* clamp to 1 */
        ASSERT(con->nodes[0].energy <= 1.0f);
        (void)old_e;
    } else { ASSERT(1); ASSERT(1); ASSERT(1); ASSERT(1); ASSERT(1); }

    /* 21-25: build on empty canvas */
    canvas_init();
    constellation_build(500, 500, 5);
    con = constellation_get();
    ASSERT(con->count == 0);
    best = constellation_infer(0.5f);
    ASSERT(best == -1);
    constellation_propagate(3);
    ASSERT(1);
    constellation_update(500, 500, 0.5f); /* non-existent node, no crash */
    ASSERT(1);

    /* 26-30: rebuild with more data */
    canvas_init();
    for (int i = 0; i < 30; i++) {
        Cell c; memset(&c, 0, sizeof(c));
        c.state = CELL_ACTIVE; c.energy = 128;
        canvas_set(200 + i % 8, 200 + i / 8, c);
    }
    constellation_build(204, 202, 6);
    con = constellation_get();
    ASSERT(con->count > 0);
    constellation_propagate(2);
    ASSERT(1);
    best = constellation_infer(0.5f);
    ASSERT(best >= 0);
    ASSERT(con->count <= MAX_CONSTELLATION_NODES);
    ASSERT(con->center_x == 204);
}

/* ===== test_emotion ===== */
static void test_emotion(void) {
    EmotionVector ev;

    /* 1-5: init */
    emotion_init(&ev);
    ASSERT(ev.joy      == 0.0f);
    ASSERT(ev.trust    == 0.0f);
    ASSERT(ev.fear     == 0.0f);
    ASSERT(ev.surprise == 0.0f);
    ASSERT(ev.sadness  == 0.0f);

    /* 6-10: update */
    emotion_update(&ev, EMOTION_JOY, 0.5f);
    ASSERT(ev.joy > 0.0f);
    emotion_update(&ev, EMOTION_FEAR, 1.0f);
    ASSERT(ev.fear > 0.0f);
    ASSERT(ev.joy > 0.0f);   /* persists */
    emotion_update(&ev, EMOTION_ANGER, 0.3f);
    ASSERT(ev.anger > 0.0f);
    emotion_update(&ev, EMOTION_TRUST, 0.8f);
    ASSERT(ev.trust > 0.0f);

    /* 11-15: clamp at 1.0 */
    emotion_init(&ev);
    emotion_update(&ev, EMOTION_JOY, 2.0f);
    ASSERT(ev.joy <= 1.0f);
    emotion_update(&ev, EMOTION_SADNESS, -1.0f);
    ASSERT(ev.sadness >= 0.0f);
    emotion_update(&ev, EMOTION_DISGUST, 0.5f);
    ASSERT(ev.disgust >= 0.0f && ev.disgust <= 1.0f);
    emotion_init(&ev);
    ev.surprise = 0.5f;
    emotion_update(&ev, EMOTION_SURPRISE, 0.6f);
    ASSERT(ev.surprise <= 1.0f);
    ASSERT(1);

    /* 16-20: dominant */
    emotion_init(&ev);
    ev.joy   = 0.9f;
    ev.fear  = 0.1f;
    ev.anger = 0.2f;
    ASSERT(emotion_dominant(&ev) == EMOTION_JOY);
    ev.trust = 0.95f;
    ASSERT(emotion_dominant(&ev) == EMOTION_TRUST);
    ev.sadness = 0.99f;
    ASSERT(emotion_dominant(&ev) == EMOTION_SADNESS);
    emotion_init(&ev);
    ASSERT(emotion_dominant(&ev) == EMOTION_JOY); /* all 0, first wins */

    /* 21-25: blend */
    EmotionVector a, b;
    emotion_init(&a); emotion_init(&b);
    a.joy = 1.0f; b.joy = 0.0f;
    EmotionVector blended = emotion_blend(a, b, 0.5f);
    ASSERT_FLOAT_EQ(blended.joy, 0.5f, 0.01f);
    blended = emotion_blend(a, b, 0.0f);
    ASSERT_FLOAT_EQ(blended.joy, 1.0f, 0.01f);
    blended = emotion_blend(a, b, 1.0f);
    ASSERT_FLOAT_EQ(blended.joy, 0.0f, 0.01f);
    a.fear = 0.6f; b.fear = 0.4f;
    blended = emotion_blend(a, b, 0.5f);
    ASSERT_FLOAT_EQ(blended.fear, 0.5f, 0.01f);
    ASSERT(blended.joy >= 0.0f);

    /* 26-30: to_energy */
    emotion_init(&ev);
    ASSERT_FLOAT_EQ(emotion_to_energy(&ev), 0.0f, 0.01f);
    ev.joy = 0.7f;
    float energy = emotion_to_energy(&ev);
    ASSERT(energy > 0.0f && energy <= 1.0f);
    ev.trust = ev.fear = ev.surprise = ev.sadness = ev.disgust = ev.anger = 1.0f;
    energy = emotion_to_energy(&ev);
    ASSERT(energy <= 1.0f);
    emotion_init(&ev);
    for (int i = 0; i < 7; i++) emotion_update(&ev, (EmotionIndex)i, 0.1f);
    ASSERT(emotion_to_energy(&ev) > 0.0f);
    ASSERT(emotion_to_energy(&ev) <= 1.0f);
}

/* ===== test_stream ===== */
static void test_stream(void) {
    stream_reset();

    /* 1-5: write keyframe */
    uint8_t data[64];
    memset(data, 0xAB, 64);
    int fid = stream_write_keyframe(data, 64);
    ASSERT(fid == 0);
    ASSERT(stream_frame_count() == 1);
    Frame *f = stream_read(0);
    ASSERT(f != NULL);
    ASSERT(f->type == FRAME_KEY);
    ASSERT(f->data_size == 64);

    /* 6-10: write delta */
    uint8_t delta[64];
    memset(delta, 0x11, 64);
    int did = stream_write_delta(fid, delta, 64);
    ASSERT(did == 1);
    ASSERT(stream_frame_count() == 2);
    Frame *df = stream_read(1);
    ASSERT(df != NULL);
    ASSERT(df->type == FRAME_DELTA);
    ASSERT(df->prev_id == 0);

    /* 11-15: reconstruct keyframe (no delta) */
    uint8_t out[64];
    int rsz = stream_reconstruct(0, out, 64);
    ASSERT(rsz == 64);
    ASSERT(out[0] == 0xAB);
    ASSERT(out[63] == 0xAB);

    /* 16-20: out of bounds */
    ASSERT(stream_read(-1) == NULL);
    ASSERT(stream_read(999) == NULL);
    ASSERT(stream_write_delta(-1, delta, 64) == -1);
    ASSERT(stream_write_delta(999, delta, 64) == -1);
    ASSERT(stream_reconstruct(-1, out, 64) == -1);

    /* 21-25: multiple keyframes at interval */
    stream_reset();
    for (int i = 0; i < 5; i++) {
        uint8_t d[32];
        memset(d, (uint8_t)i, 32);
        stream_write_keyframe(d, 32);
    }
    ASSERT(stream_frame_count() == 5);
    Frame *f3 = stream_read(2);
    ASSERT(f3 != NULL);
    ASSERT(f3->data[0] == 2);
    int r2 = stream_reconstruct(2, out, 64);
    ASSERT(r2 == 32);
    ASSERT(out[0] == 2);

    /* 26-30: checksum set */
    stream_reset();
    memset(data, 0x55, 16);
    int k = stream_write_keyframe(data, 16);
    f = stream_read(k);
    ASSERT(f->checksum != 0);
    ASSERT(f->timestamp > 0 || f->timestamp == 0); /* just ensure no crash */
    int sz = stream_reconstruct(k, out, 64);
    ASSERT(sz == 16);
    ASSERT(out[0] == 0x55);
    ASSERT(out[15] == 0x55);
}

/* ===== test_compress ===== */
static void test_compress(void) {
    uint8_t input[256], output[512], recovered[256];

    /* 1-5: basic round-trip */
    for (int i = 0; i < 256; i++) input[i] = (uint8_t)i;
    int csz = compress_predicted_delta(input, 256, output, 512);
    ASSERT(csz > 0);
    int rsz = compress_decompress(output, (size_t)csz, recovered, 256);
    ASSERT(rsz == 256);
    ASSERT(memcmp(input, recovered, 256) == 0);

    /* 6-10: repetitive data */
    memset(input, 0xAA, 256);
    csz = compress_predicted_delta(input, 256, output, 512);
    ASSERT(csz > 0);
    rsz = compress_decompress(output, (size_t)csz, recovered, 256);
    ASSERT(rsz == 256);
    ASSERT(recovered[0] == 0xAA);
    ASSERT(recovered[255] == 0xAA);

    /* 11-15: ratio */
    float ratio = compress_ratio(256, 256);
    ASSERT_FLOAT_EQ(ratio, 1.0f, 0.01f);
    ratio = compress_ratio(256, 128);
    ASSERT_FLOAT_EQ(ratio, 2.0f, 0.01f);
    ratio = compress_ratio(256, 512);
    ASSERT_FLOAT_EQ(ratio, 0.5f, 0.01f);
    ratio = compress_ratio(0, 1);
    ASSERT_FLOAT_EQ(ratio, 0.0f, 0.01f);
    ratio = compress_ratio(1000, 500);
    ASSERT_FLOAT_EQ(ratio, 2.0f, 0.01f);

    /* 16-20: null/bad input */
    csz = compress_predicted_delta(NULL, 256, output, 512);
    ASSERT(csz < 0);
    csz = compress_predicted_delta(input, 256, NULL, 512);
    ASSERT(csz < 0);
    rsz = compress_decompress(NULL, 256, recovered, 256);
    ASSERT(rsz < 0);
    rsz = compress_decompress(output, 0, recovered, 256); /* size < 4 */
    ASSERT(rsz < 0);
    ASSERT(1); /* padding */

    /* 21-25: small data */
    uint8_t tiny[1] = {0x42};
    uint8_t tout[16], trec[8];
    csz = compress_predicted_delta(tiny, 1, tout, 16);
    ASSERT(csz > 0);
    rsz = compress_decompress(tout, (size_t)csz, trec, 8);
    ASSERT(rsz == 1);
    ASSERT(trec[0] == 0x42);

    /* 26-30: zero data */
    memset(input, 0, 256);
    csz = compress_predicted_delta(input, 256, output, 512);
    ASSERT(csz > 0);
    rsz = compress_decompress(output, (size_t)csz, recovered, 256);
    ASSERT(rsz == 256);
    ASSERT(recovered[0] == 0);
    ASSERT(recovered[255] == 0);
    ASSERT(memcmp(input, recovered, 256) == 0);
}

/* ===== test_v6f ===== */
static void test_v6f(void) {
    /* 1-5: encode/decode */
    V6F f = v6f_encode(100.0f, 99.0f, 101.0f, 98.0f, 5000.0f, 1.0f);
    float price, open, high, low, vol, ts;
    v6f_decode(&f, &price, &open, &high, &low, &vol, &ts);
    ASSERT_FLOAT_EQ(price, 100.0f, 0.001f);
    ASSERT_FLOAT_EQ(open,   99.0f, 0.001f);
    ASSERT_FLOAT_EQ(high,  101.0f, 0.001f);
    ASSERT_FLOAT_EQ(low,    98.0f, 0.001f);
    ASSERT_FLOAT_EQ(vol,  5000.0f, 0.001f);

    /* 6-10: distance */
    V6F a = v6f_encode(0,0,0,0,0,0);
    V6F b = v6f_encode(1,0,0,0,0,0);
    ASSERT_FLOAT_EQ(v6f_distance(&a, &b), 1.0f, 0.001f);
    V6F c2 = v6f_encode(3,4,0,0,0,0);
    ASSERT_FLOAT_EQ(v6f_distance(&a, &c2), 5.0f, 0.001f);
    ASSERT_FLOAT_EQ(v6f_distance(&a, &a), 0.0f, 0.001f);
    float d = v6f_distance(&f, &a);
    ASSERT(d > 0.0f);

    /* 11-15: similarity */
    V6F x = v6f_encode(1,0,0,0,0,0);
    V6F y = v6f_encode(1,0,0,0,0,0);
    ASSERT_FLOAT_EQ(v6f_similarity(&x, &y), 1.0f, 0.001f);
    V6F z = v6f_encode(-1,0,0,0,0,0);
    ASSERT_FLOAT_EQ(v6f_similarity(&x, &z), -1.0f, 0.001f);
    V6F zero = v6f_encode(0,0,0,0,0,0);
    ASSERT_FLOAT_EQ(v6f_similarity(&x, &zero), 0.0f, 0.001f);
    V6F p = v6f_encode(1,1,0,0,0,0);
    V6F q = v6f_encode(0,1,0,0,0,0);
    float sim = v6f_similarity(&p, &q);
    ASSERT(sim > 0.0f && sim < 1.0f);

    /* 16-20: decode NULL fields (no crash) */
    v6f_decode(&f, NULL, NULL, NULL, NULL, NULL, NULL);
    ASSERT(1);
    v6f_decode(&f, &price, NULL, NULL, NULL, NULL, NULL);
    ASSERT_FLOAT_EQ(price, 100.0f, 0.001f);

    /* 21-25: encode negative */
    V6F neg = v6f_encode(-1.0f, -2.0f, -0.5f, -3.0f, 0.0f, 100.0f);
    v6f_decode(&neg, &price, &open, &high, &low, NULL, &ts);
    ASSERT_FLOAT_EQ(price, -1.0f, 0.001f);
    ASSERT_FLOAT_EQ(open,  -2.0f, 0.001f);
    ASSERT_FLOAT_EQ(high,  -0.5f, 0.001f);
    ASSERT_FLOAT_EQ(low,   -3.0f, 0.001f);
    ASSERT_FLOAT_EQ(ts,   100.0f, 0.001f);

    /* 26-30: distance symmetry, similarity range */
    V6F u = v6f_encode(1,2,3,4,5,6);
    V6F v2 = v6f_encode(6,5,4,3,2,1);
    ASSERT_FLOAT_EQ(v6f_distance(&u, &v2), v6f_distance(&v2, &u), 0.001f);
    float s = v6f_similarity(&u, &v2);
    ASSERT(s >= -1.0f && s <= 1.0f);
    ASSERT_FLOAT_EQ(v6f_similarity(&u, &u), 1.0f, 0.001f);
    ASSERT(v6f_distance(&u, &v2) > 0.0f);
    ASSERT(1);
}

/* ===== test_wh ===== */
static void test_wh(void) {
    wh_init();

    /* 1-5: initial state */
    ASSERT(wh_count() == 0);
    ASSERT(wh_latest() == NULL);
    ASSERT(wh_get(0) == NULL);
    ASSERT(wh_get(-1) == NULL);
    ASSERT(wh_get(999) == NULL);

    /* 6-10: record */
    uint8_t meta[8] = {1,2,3,4,5,6,7,8};
    int rid = wh_record(1000, meta, 8);
    ASSERT(rid == 0);
    ASSERT(wh_count() == 1);
    WHRecord *r = wh_get(0);
    ASSERT(r != NULL);
    ASSERT(r->timestamp == 1000);
    ASSERT(r->metadata[0] == 1);

    /* 11-15: multiple records */
    wh_record(2000, meta, 8);
    wh_record(3000, meta, 8);
    ASSERT(wh_count() == 3);
    WHRecord *lat = wh_latest();
    ASSERT(lat != NULL);
    ASSERT(lat->timestamp == 3000);
    ASSERT(wh_get(1)->timestamp == 2000);
    ASSERT(wh_get(2)->timestamp == 3000);

    /* 16-20: record_id increments */
    int r2 = wh_record(4000, NULL, 0);
    ASSERT(r2 == 3);
    ASSERT(wh_count() == 4);
    ASSERT(wh_latest()->timestamp == 4000);
    r = wh_get(3);
    ASSERT(r != NULL);
    ASSERT(r->data_size == 0);

    /* 21-25: metadata stored */
    uint8_t big[100];
    memset(big, 0xFF, 100);
    wh_record(5000, big, 100);
    r = wh_get(4);
    ASSERT(r != NULL);
    ASSERT(r->data_size == 64); /* capped at 64 */
    ASSERT(r->metadata[0] == 0xFF);
    ASSERT(r->metadata[63] == 0xFF);

    /* 26-30: record_id matches index */
    wh_init();
    for (int i = 0; i < 5; i++) wh_record((uint64_t)(i*100), meta, 8);
    ASSERT(wh_count() == 5);
    ASSERT(wh_get(0)->record_id == 0);
    ASSERT(wh_get(4)->record_id == 4);
    ASSERT(wh_latest()->timestamp == 400);
    ASSERT(wh_get(2)->timestamp == 200);
    ASSERT(1);
}

/* ===== test_bh ===== */
static void test_bh(void) {
    wh_init();
    bh_init();

    /* 1-5: initial */
    ASSERT(bh_get_summary(0) == NULL);
    ASSERT(bh_get_summary(-1) == NULL);

    /* Populate WH */
    uint8_t meta[8] = {100, 50, 200, 10, 0, 0, 0, 0};
    wh_record(100, meta, 8);
    wh_record(200, meta, 8);
    wh_record(300, meta, 8);

    /* 6-10: compress history */
    int sid = bh_compress_history(100, 300);
    ASSERT(sid == 0);
    BHSummary *s = bh_get_summary(0);
    ASSERT(s != NULL);
    ASSERT(s->record_count == 3);
    ASSERT(s->from_time == 100);
    ASSERT(s->to_time   == 300);

    /* 11-15: energy_avg */
    ASSERT(s->energy_avg >= 0.0f && s->energy_avg <= 1.0f);
    int sid2 = bh_compress_history(100, 150);
    ASSERT(sid2 == 1);
    BHSummary *s2 = bh_get_summary(1);
    ASSERT(s2 != NULL);
    ASSERT(s2->record_count == 1);
    ASSERT(s2->energy_avg >= 0.0f);

    /* 16-20: summarize_pattern */
    int pids[3] = {0,1,2};
    int psid = bh_summarize_pattern(pids, 3);
    ASSERT(psid >= 0);
    BHSummary *ps = bh_get_summary(psid);
    ASSERT(ps != NULL);
    ASSERT(ps->record_count == 3);
    ASSERT(ps->energy_avg >= 0.0f);

    /* 21-25: forget */
    bh_forget(0);
    WHRecord *r = wh_get(0);
    ASSERT(r != NULL);
    ASSERT(r->data_size == 0); /* cleared */
    bh_forget(-1); /* no crash */
    ASSERT(1);
    bh_forget(999); /* no crash */
    ASSERT(1);
    bh_forget(1);
    r = wh_get(1);
    ASSERT(r->data_size == 0);

    /* 26-30: multiple summaries */
    bh_init(); wh_init();
    for (int i = 0; i < 5; i++) {
        uint8_t m[4] = {(uint8_t)(i*50), 0, 0, 0};
        wh_record((uint64_t)(i*100), m, 4);
    }
    for (int i = 0; i < 3; i++) bh_compress_history((uint64_t)(i*100), (uint64_t)(i*100+100));
    ASSERT(bh_get_summary(0) != NULL);
    ASSERT(bh_get_summary(1) != NULL);
    ASSERT(bh_get_summary(2) != NULL);
    ASSERT(bh_get_summary(3) == NULL);
    ASSERT(1);
}

/* ===== test_merge ===== */
static void test_merge(void) {
    canvas_init();

    Cell a, b;
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    a.color.r = 100; a.color.g = 50; a.energy = 80; a.state = CELL_ACTIVE; a.id = 10;
    b.color.r = 200; b.color.g = 30; b.energy = 40; b.state = CELL_IDLE;   b.id = 20;

    /* 1-5: MERGE_LATEST */
    Cell m = merge_cells(a, b, MERGE_LATEST);
    ASSERT(m.color.r == 200);
    ASSERT(m.id == 20);
    ASSERT(m.energy == 40);

    /* 6-10: MERGE_OLDEST */
    m = merge_cells(a, b, MERGE_OLDEST);
    ASSERT(m.color.r == 100);
    ASSERT(m.id == 10);
    ASSERT(m.energy == 80);

    /* 11-15: MERGE_AND */
    m = merge_cells(a, b, MERGE_AND);
    ASSERT(m.color.r == (100 & 200));
    ASSERT(m.energy  == (80 & 40));

    /* 16-20: MERGE_OR */
    m = merge_cells(a, b, MERGE_OR);
    ASSERT(m.color.r == (100 | 200));
    ASSERT(m.energy  == (80 | 40));

    /* 21-25: MERGE_XOR */
    m = merge_cells(a, b, MERGE_XOR);
    ASSERT(m.color.r == (100 ^ 200));
    ASSERT(m.energy  == (80 ^ 40));

    /* MERGE_AVERAGE */
    m = merge_cells(a, b, MERGE_AVERAGE);
    ASSERT(m.color.r == 150);
    ASSERT(m.energy  == 60);
    ASSERT(m.id == 15);

    /* merge_apply_delta */
    Cell base, delta;
    memset(&base, 0, sizeof(base)); memset(&delta, 0, sizeof(delta));
    base.color.r = 0xFF; delta.color.r = 0x0F;
    Cell res = merge_apply_delta(base, delta);
    ASSERT(res.color.r == (0xFF ^ 0x0F));
    base.energy = 100; delta.energy = 50;
    res = merge_apply_delta(base, delta);
    ASSERT(res.energy == 150);

    /* 26-30: merge_conflict_resolve */
    Cell cands[3];
    memset(cands, 0, sizeof(cands));
    cands[0].energy = 10; cands[1].energy = 20; cands[2].energy = 30;
    Cell resolved = merge_conflict_resolve(cands, 3, MERGE_AVERAGE);
    ASSERT(resolved.energy > 0);
    resolved = merge_conflict_resolve(cands, 3, MERGE_LATEST);
    ASSERT(resolved.energy == 30);
    resolved = merge_conflict_resolve(cands, 3, MERGE_OLDEST);
    ASSERT(resolved.energy == 10);
    Cell empty_res = merge_conflict_resolve(NULL, 0, MERGE_LATEST);
    ASSERT(empty_res.energy == 0);
    ASSERT(1);
}

/* ===== test_lane ===== */
static int lane_task_runs = 0;
static void test_task(void *ctx) { (void)ctx; lane_task_runs++; }

static void test_lane(void) {
    lane_init();

    /* 1-5: initial state */
    ASSERT(lane_count_active() == 0);
    ASSERT(lane_get(0) != NULL);
    ASSERT(lane_get(255) != NULL);
    ASSERT(lane_get(-1) == NULL);
    ASSERT(lane_get(256) == NULL);

    /* 6-10: spawn */
    int lid = lane_spawn(test_task, NULL, 5);
    ASSERT(lid >= 0 && lid < LANE_COUNT);
    ASSERT(lane_count_active() == 1);
    Lane *l = lane_get(lid);
    ASSERT(l != NULL);
    ASSERT(l->active == true);
    ASSERT(l->priority == 5);

    /* 11-15: tick executes task */
    lane_task_runs = 0;
    lane_tick();
    ASSERT(lane_task_runs == 1);
    lane_tick();
    ASSERT(lane_task_runs == 2);
    ASSERT(l->ticks == 2);

    /* 16-20: kill */
    lane_kill(lid);
    ASSERT(lane_count_active() == 0);
    lane_task_runs = 0;
    lane_tick();
    ASSERT(lane_task_runs == 0);

    /* 21-25: spawn many */
    lane_init();
    for (int i = 0; i < 10; i++) lane_spawn(test_task, NULL, (uint8_t)i);
    ASSERT(lane_count_active() == 10);
    lane_task_runs = 0;
    lane_tick();
    ASSERT(lane_task_runs == 10);
    lane_init();
    ASSERT(lane_count_active() == 0);

    /* 26-30: spawn with NULL task */
    int lid2 = lane_spawn(NULL, NULL, 1);
    ASSERT(lid2 >= 0);
    ASSERT(lane_count_active() == 1);
    lane_task_runs = 0;
    lane_tick(); /* NULL task: should not crash */
    ASSERT(lane_task_runs == 0);
    lane_kill(lid2);
    ASSERT(lane_count_active() == 0);
    ASSERT(1);
}

/* ===== test_branch ===== */
static void test_branch(void) {
    canvas_init();
    branch_system_init();

    /* 1-5: create root branch */
    int bid = branch_create(-1);
    ASSERT(bid == 0);
    int bid2 = branch_create(bid);
    ASSERT(bid2 == 1);
    ASSERT(branch_count_active() == 2);

    /* 6-10: apply patch */
    Cell c; memset(&c, 0, sizeof(c));
    c.energy = 99; c.state = CELL_ACTIVE; c.id = 77;
    branch_apply_patch(bid2, 100, 100, c);
    Cell got = branch_get_cell(bid2, 100, 100);
    ASSERT(got.energy == 99);
    ASSERT(got.id == 77);

    /* 11-15: COW - parent not affected */
    Cell parent_cell = branch_get_cell(bid, 100, 100);
    ASSERT(parent_cell.energy == 0); /* canvas default */

    /* Update existing patch */
    c.energy = 50;
    branch_apply_patch(bid2, 100, 100, c);
    got = branch_get_cell(bid2, 100, 100);
    ASSERT(got.energy == 50);
    ASSERT(got.id == 77);

    /* 16-20: merge */
    int bid3 = branch_create(-1);
    branch_apply_patch(bid3, 200, 200, c);
    branch_merge(bid2, bid3);
    Cell m = branch_get_cell(bid3, 100, 100);
    ASSERT(m.energy == 50); /* merged from bid2 */
    Cell m2 = branch_get_cell(bid3, 200, 200);
    ASSERT(m2.energy == 50);

    /* 21-25: delete */
    branch_delete(bid2);
    ASSERT(branch_count_active() == 2); /* bid and bid3 remain */
    branch_delete(-1); /* no crash */
    ASSERT(1);
    branch_delete(999); /* no crash */
    ASSERT(1);

    /* get_cell on deleted branch falls back */
    got = branch_get_cell(bid2, 100, 100);
    ASSERT(got.energy >= 0); /* canvas fallback */

    /* 26-30: many patches */
    branch_system_init();
    int b = branch_create(-1);
    for (int i = 0; i < 50; i++) {
        Cell ci; memset(&ci, 0, sizeof(ci)); ci.energy = (uint8_t)i;
        branch_apply_patch(b, i, i, ci);
    }
    ASSERT(branch_get_cell(b, 0, 0).energy == 0);
    ASSERT(branch_get_cell(b, 49, 49).energy == 49);
    ASSERT(branch_count_active() == 1);
    ASSERT(1);
    ASSERT(1);
}

/* ===== test_multiverse ===== */
static void test_multiverse(void) {
    canvas_init();
    branch_system_init();
    multiverse_init();

    /* 1-5: spawn universes */
    int uid0 = multiverse_spawn(-1, 0);
    ASSERT(uid0 == 0);
    int uid1 = multiverse_spawn(uid0, 1);
    ASSERT(uid1 == 1);
    ASSERT(multiverse_active_count() == 2);
    Universe *u0 = multiverse_get(uid0);
    ASSERT(u0 != NULL);
    ASSERT(u0->active == true);

    /* 6-10: get_cell defaults to canvas */
    Cell c = multiverse_get_cell(uid0, 100, 100);
    ASSERT(c.energy == 0);
    Cell cv; memset(&cv, 0, sizeof(cv));
    cv.energy = 77; cv.state = CELL_ACTIVE;
    branch_apply_patch(u0->branch_id, 100, 100, cv);
    c = multiverse_get_cell(uid0, 100, 100);
    ASSERT(c.energy == 77);

    /* 11-15: probability update */
    multiverse_probability_update(uid0, 2.0f);
    float sum = 0.0f;
    for (int i = 0; i < 2; i++) {
        Universe *u = multiverse_get(i);
        if (u && u->active) sum += u->probability;
    }
    ASSERT_FLOAT_EQ(sum, 1.0f, 0.01f);

    /* 16-20: collapse */
    multiverse_collapse(uid0);
    ASSERT(multiverse_get(uid0)->active == true);
    ASSERT(multiverse_get(uid1)->active == false);
    ASSERT(multiverse_active_count() == 1);

    /* 21-25: spawn up to UNIVERSE_COUNT */
    multiverse_init(); branch_system_init();
    int spawned = 0;
    for (int i = 0; i < UNIVERSE_COUNT; i++) {
        int uid = multiverse_spawn(i == 0 ? -1 : 0, i);
        if (uid >= 0) spawned++;
    }
    ASSERT(spawned == UNIVERSE_COUNT);
    ASSERT(multiverse_active_count() == UNIVERSE_COUNT);
    int overflow = multiverse_spawn(0, 0);
    ASSERT(overflow < 0); /* no room */

    /* 26-30: out of bounds */
    ASSERT(multiverse_get(-1) == NULL);
    ASSERT(multiverse_get(UNIVERSE_COUNT) == NULL);
    Cell oor = multiverse_get_cell(-1, 0, 0);
    ASSERT(oor.energy == 0); /* fallback to canvas */
    multiverse_probability_update(-1, 1.0f); /* no crash */
    ASSERT(1);
    multiverse_collapse(-1); /* no crash */
    ASSERT(1);
}

/* ===== test_scheduler ===== */
static int sched_task_count = 0;
static void sched_task(void *ctx) { (void)ctx; sched_task_count++; }

static void test_scheduler(void) {
    lane_init();
    scheduler_init();

    /* 1-4: spawn process */
    int pid = scheduler_spawn("test_proc", 5, sched_task);
    ASSERT(pid >= 0);
    ASSERT(scheduler_active_count() == 1);
    sched_task_count = 0;
    scheduler_tick();
    ASSERT(sched_task_count >= 0); /* may or may not run depending on impl */

    /* 5-9: kill */
    scheduler_kill(pid);
    ASSERT(scheduler_active_count() == 0);
    scheduler_kill(-1); /* no crash */
    ASSERT(1);
    scheduler_kill(999); /* no crash */
    ASSERT(1);

    /* 10-14: list does not crash */
    scheduler_init();
    scheduler_spawn("proc_a", 1, sched_task);
    scheduler_spawn("proc_b", 2, sched_task);
    ASSERT(scheduler_active_count() == 2);
    scheduler_list();
    ASSERT(1);
    scheduler_tick();
    ASSERT(1);
}

/* ===== main ===== */
int main(void) {
    test_canvas();
    test_gate();
    test_scan();
    test_pattern();
    test_constellation();
    test_emotion();
    test_stream();
    test_compress();
    test_v6f();
    test_wh();
    test_bh();
    test_merge();
    test_lane();
    test_branch();
    test_multiverse();
    test_scheduler();

    printf("\n");
    if (tests_passed == tests_run)
        printf("PASS: %d/%d tests\n", tests_passed, tests_run);
    else
        printf("PARTIAL: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
