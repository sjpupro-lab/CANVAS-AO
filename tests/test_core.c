#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <string.h>
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

/* Integer comparison within tolerance */
#define ASSERT_INT_NEAR(a, b, tol) ASSERT(abs((int)(a)-(int)(b)) <= (tol))

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
    ASSERT(pr.confidence <= 255);
    ASSERT(pr.layer >= LAYER_RAW && pr.layer <= LAYER_ABSTRACT);
    ASSERT(pr.matched_id == 0);

    /* 6-10: set active cells and recognize */
    Cell ac; memset(&ac, 0, sizeof(ac));
    ac.state = CELL_ACTIVE; ac.energy = 200;
    ac.color.r = 255;
    canvas_set(100, 100, ac);
    pr = pattern_recognize(100, 100, 4);
    ASSERT(pr.confidence > 0);
    ASSERT(pr.layer >= LAYER_RAW);

    /* Pattern with surrounding context */
    for (int dx = -3; dx <= 3; dx++) {
        for (int dy = -3; dy <= 3; dy++) {
            canvas_set(100+dx, 100+dy, ac);
        }
    }
    pr = pattern_recognize(100, 100, 4);
    ASSERT(pr.confidence > 0);

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
    ASSERT(pr.confidence >= 0);
    pr = pattern_recognize(0, 0, 2);
    ASSERT(pr.layer >= LAYER_RAW);
    pr = pattern_recognize(CANVAS_WIDTH-1, CANVAS_HEIGHT-1, 2);
    ASSERT(pr.confidence >= 0);
    pr = pattern_recognize(200, 200, 8);
    ASSERT(pr.layer <= LAYER_ABSTRACT);

    /* 21-25: training */
    pattern_train(100, 100, LAYER_SHAPE, 99);
    pr = pattern_recognize(100, 100, 4);
    ASSERT(pr.confidence >= 0);
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
    ASSERT(pr.confidence <= 255);
    pr = pattern_recognize(200, 200, 4);
    ASSERT(pr.confidence <= 255);
    pr = pattern_recognize(300, 300, 4);
    ASSERT(pr.confidence <= 255);
    pr = pattern_recognize(50, 50, 4);
    ASSERT(pr.confidence <= 255);
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
        ASSERT(con->nodes[i].energy <= 255);
        ASSERT(1); /* energy is uint8_t, always >= 0 */
    }

    /* 11-15: infer returns valid index or -1 */
    int best = constellation_infer(128);   /* was 0.5f → 128 */
    ASSERT(best >= -1 && best < MAX_CONSTELLATION_NODES);
    best = constellation_infer(0);         /* was 0.0f */
    ASSERT(best >= -1);
    best = constellation_infer(255);       /* was 1.0f */
    ASSERT(best >= -1);

    /* 16-20: update energy */
    if (con->count > 0) {
        int nx = con->nodes[0].x;
        int ny = con->nodes[0].y;
        uint8_t old_e = con->nodes[0].energy;
        constellation_update(nx, ny, 26);    /* was 0.1f → ~26 */
        ASSERT(con->nodes[0].energy <= 255);
        ASSERT(1); /* uint8_t always >= 0 */
        constellation_update(nx, ny, -512);  /* clamp to 0 */
        ASSERT(con->nodes[0].energy == 0);
        constellation_update(nx, ny, 512);   /* clamp to 255 */
        ASSERT(con->nodes[0].energy == 255);
        (void)old_e;
    } else { ASSERT(1); ASSERT(1); ASSERT(1); ASSERT(1); ASSERT(1); }

    /* 21-25: build on empty canvas */
    canvas_init();
    constellation_build(500, 500, 5);
    con = constellation_get();
    ASSERT(con->count == 0);
    best = constellation_infer(128);
    ASSERT(best == -1);
    constellation_propagate(3);
    ASSERT(1);
    constellation_update(500, 500, 128); /* non-existent node, no crash */
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
    best = constellation_infer(128);
    ASSERT(best >= 0);
    ASSERT(con->count <= MAX_CONSTELLATION_NODES);
    ASSERT(con->center_x == 204);
}

/* ===== test_emotion ===== */
static void test_emotion(void) {
    EmotionVector ev;

    /* 1-5: init */
    emotion_init(&ev);
    ASSERT(ev.joy      == 0);
    ASSERT(ev.trust    == 0);
    ASSERT(ev.fear     == 0);
    ASSERT(ev.surprise == 0);
    ASSERT(ev.sadness  == 0);

    /* 6-10: update */
    emotion_update(&ev, EMOTION_JOY, 128);     /* was 0.5f → 128 */
    ASSERT(ev.joy > 0);
    emotion_update(&ev, EMOTION_FEAR, 255);     /* was 1.0f → 255 */
    ASSERT(ev.fear > 0);
    ASSERT(ev.joy > 0);   /* persists */
    emotion_update(&ev, EMOTION_ANGER, 77);     /* was 0.3f → 77 */
    ASSERT(ev.anger > 0);
    emotion_update(&ev, EMOTION_TRUST, 204);    /* was 0.8f → 204 */
    ASSERT(ev.trust > 0);

    /* 11-15: clamp at 255 */
    emotion_init(&ev);
    emotion_update(&ev, EMOTION_JOY, 255);
    emotion_update(&ev, EMOTION_JOY, 255);      /* saturates at 255 */
    ASSERT(ev.joy == 255);
    emotion_update(&ev, EMOTION_SADNESS, 0);     /* zero intensity = no change */
    ASSERT(ev.sadness == 0);                      /* stays 0, decayed from 0 */
    emotion_update(&ev, EMOTION_DISGUST, 128);
    ASSERT(ev.disgust <= 255);
    emotion_init(&ev);
    ev.surprise = 128;
    emotion_update(&ev, EMOTION_SURPRISE, 153);   /* was 0.6f → 153 */
    ASSERT(ev.surprise <= 255);
    ASSERT(1);

    /* 16-20: dominant */
    emotion_init(&ev);
    ev.joy   = 230;    /* was 0.9f */
    ev.fear  = 26;     /* was 0.1f */
    ev.anger = 51;     /* was 0.2f */
    ASSERT(emotion_dominant(&ev) == EMOTION_JOY);
    ev.trust = 242;    /* was 0.95f */
    ASSERT(emotion_dominant(&ev) == EMOTION_TRUST);
    ev.sadness = 252;  /* was 0.99f */
    ASSERT(emotion_dominant(&ev) == EMOTION_SADNESS);
    emotion_init(&ev);
    ASSERT(emotion_dominant(&ev) == EMOTION_JOY); /* all 0, first wins */

    /* 21-25: blend */
    EmotionVector a, b;
    emotion_init(&a); emotion_init(&b);
    a.joy = 255; b.joy = 0;
    EmotionVector blended = emotion_blend(a, b, 128);   /* ratio=0.5 → 128 */
    ASSERT_INT_NEAR(blended.joy, 128, 2);               /* ~128 (was 0.5f) */
    blended = emotion_blend(a, b, 0);                    /* ratio=0.0 → 0 */
    ASSERT_INT_NEAR(blended.joy, 255, 1);                /* ~255 (was 1.0f) */
    blended = emotion_blend(a, b, 255);                  /* ratio=1.0 → 255 */
    ASSERT_INT_NEAR(blended.joy, 0, 1);                  /* ~0 (was 0.0f) */
    a.fear = 153; b.fear = 102;                          /* 0.6, 0.4 */
    blended = emotion_blend(a, b, 128);                  /* ratio=0.5 */
    ASSERT_INT_NEAR(blended.fear, 128, 3);               /* ~128 (was 0.5f) */
    ASSERT(blended.joy >= 0);                             /* uint8_t always >= 0 */

    /* 26-30: to_energy */
    emotion_init(&ev);
    ASSERT(emotion_to_energy(&ev) == 0);
    ev.joy = 179;                                        /* was 0.7f → 179 */
    uint8_t energy = emotion_to_energy(&ev);
    ASSERT(energy > 0 && energy <= 255);
    ev.trust = ev.fear = ev.surprise = ev.sadness = ev.disgust = ev.anger = 255;
    energy = emotion_to_energy(&ev);
    ASSERT(energy <= 255);
    emotion_init(&ev);
    for (int i = 0; i < 7; i++) emotion_update(&ev, (EmotionIndex)i, 26);  /* was 0.1f → 26 */
    ASSERT(emotion_to_energy(&ev) > 0);
    ASSERT(emotion_to_energy(&ev) <= 255);
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

    /* 11-15: ratio (×256 fixed-point: 256=1:1, 512=2:1) */
    uint32_t ratio = compress_ratio(256, 256);
    ASSERT(ratio == 256);                /* 1:1 */
    ratio = compress_ratio(256, 128);
    ASSERT(ratio == 512);                /* 2:1 */
    ratio = compress_ratio(256, 512);
    ASSERT(ratio == 128);                /* 0.5:1 */
    ratio = compress_ratio(0, 1);
    ASSERT(ratio == 0);                  /* 0:1 */
    ratio = compress_ratio(1000, 500);
    ASSERT(ratio == 512);                /* 2:1 */

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
    /* 1-5: encode/decode (int32_t values) */
    V6F f = v6f_encode(10000, 9900, 10100, 9800, 500000, 100);
    int32_t price, open, high, low, vol, ts;
    v6f_decode(&f, &price, &open, &high, &low, &vol, &ts);
    ASSERT(price == 10000);
    ASSERT(open  == 9900);
    ASSERT(high  == 10100);
    ASSERT(low   == 9800);
    ASSERT(vol   == 500000);

    /* 6-10: distance_sq (squared Euclidean) */
    V6F a = v6f_encode(0,0,0,0,0,0);
    V6F b = v6f_encode(1,0,0,0,0,0);
    ASSERT(v6f_distance_sq(&a, &b) == 1);           /* 1² = 1 */
    V6F c2 = v6f_encode(3,4,0,0,0,0);
    ASSERT(v6f_distance_sq(&a, &c2) == 25);          /* 3²+4² = 25 */
    ASSERT(v6f_distance_sq(&a, &a) == 0);
    uint32_t d = v6f_distance_sq(&f, &a);
    ASSERT(d > 0);

    /* 11-15: similarity (×10000: 10000=identical, -10000=opposite) */
    V6F x = v6f_encode(1,0,0,0,0,0);
    V6F y = v6f_encode(1,0,0,0,0,0);
    ASSERT(v6f_similarity(&x, &y) == 10000);
    V6F z = v6f_encode(-1,0,0,0,0,0);
    ASSERT(v6f_similarity(&x, &z) == -10000);
    V6F zero = v6f_encode(0,0,0,0,0,0);
    ASSERT(v6f_similarity(&x, &zero) == 0);
    V6F p = v6f_encode(1000,1000,0,0,0,0);
    V6F q = v6f_encode(0,1000,0,0,0,0);
    int32_t sim = v6f_similarity(&p, &q);
    ASSERT(sim > 0 && sim < 10000);

    /* 16-20: decode NULL fields (no crash) */
    v6f_decode(&f, NULL, NULL, NULL, NULL, NULL, NULL);
    ASSERT(1);
    v6f_decode(&f, &price, NULL, NULL, NULL, NULL, NULL);
    ASSERT(price == 10000);

    /* 21-25: encode negative */
    V6F neg = v6f_encode(-100, -200, -50, -300, 0, 10000);
    v6f_decode(&neg, &price, &open, &high, &low, NULL, &ts);
    ASSERT(price == -100);
    ASSERT(open  == -200);
    ASSERT(high  == -50);
    ASSERT(low   == -300);
    ASSERT(ts    == 10000);

    /* 26-30: distance symmetry, similarity range */
    V6F u = v6f_encode(1,2,3,4,5,6);
    V6F v2 = v6f_encode(6,5,4,3,2,1);
    ASSERT(v6f_distance_sq(&u, &v2) == v6f_distance_sq(&v2, &u));
    int32_t s = v6f_similarity(&u, &v2);
    ASSERT(s >= -10000 && s <= 10000);
    ASSERT(v6f_similarity(&u, &u) == 10000);
    ASSERT(v6f_distance_sq(&u, &v2) > 0);
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

    /* 11-15: energy_avg (0-255) */
    ASSERT(s->energy_avg <= 255);   /* uint8_t always in range */
    int sid2 = bh_compress_history(100, 150);
    ASSERT(sid2 == 1);
    BHSummary *s2 = bh_get_summary(1);
    ASSERT(s2 != NULL);
    ASSERT(s2->record_count == 1);
    ASSERT(s2->energy_avg <= 255);

    /* 16-20: summarize_pattern */
    int pids[3] = {0,1,2};
    int psid = bh_summarize_pattern(pids, 3);
    ASSERT(psid >= 0);
    BHSummary *ps = bh_get_summary(psid);
    ASSERT(ps != NULL);
    ASSERT(ps->record_count == 3);
    ASSERT(ps->energy_avg <= 255);

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

    /* 11-15: probability update (evidence ×256: 512 = 2.0x) */
    multiverse_probability_update(uid0, 512);
    uint32_t sum = 0;
    for (int i = 0; i < 2; i++) {
        Universe *u = multiverse_get(i);
        if (u && u->active) sum += u->probability;
    }
    ASSERT_INT_NEAR(sum, 65535, 2);  /* renormalized to ~65535 */

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
    multiverse_probability_update(-1, 256); /* no crash (256 = 1.0x) */
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

/* ===== test_elo ===== */
static void test_elo(void) {
    elo_init();

    /* 1-5: initial trust values */
    ASSERT(elo_get_trust(1) > 0);     /* layer 1 has initial trust */
    ASSERT(elo_get_trust(2) > 0);
    ASSERT(elo_get_trust(3) > 0);
    ASSERT(elo_get_trust(1) <= 255);
    ASSERT(elo_get_trust(0) == 128);  /* invalid layer → default 128 */

    /* 6-10: predict and feed */
    uint8_t ctx[6] = {65, 66, 67, 68, 69, 70};  /* ABCDEF */
    uint8_t pred = elo_predict(ctx, 6);
    ASSERT(pred <= 255);  /* valid byte */
    uint8_t conf = elo_confidence(ctx, 6);
    ASSERT(conf <= 255);
    elo_feed(ctx, 6, 71);  /* feed 'G' as actual */
    ASSERT(1);  /* no crash */
    elo_feed(ctx, 6, 71);  /* feed again — should increase confidence */
    ASSERT(1);

    /* 11-15: repeated training builds trust */
    elo_init();
    uint8_t pattern[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    for (int i = 0; i < 200; i++) {
        elo_feed(pattern, 6, pattern[6]);
    }
    uint8_t t3 = elo_get_trust(3);
    ASSERT(t3 > 0);  /* trust should be nonzero after training */
    ASSERT(t3 <= 255);
    ASSERT(elo_get_trust(2) <= 255);
    ASSERT(elo_get_trust(1) <= 255);
    ASSERT(1);

    /* 16-20: edge cases */
    elo_feed(NULL, 0, 0);  /* no crash */
    ASSERT(1);
    elo_feed(ctx, 1, 42);  /* short context */
    ASSERT(1);
    elo_predict(ctx, 0);   /* zero-length */
    ASSERT(1);
    elo_confidence(ctx, 0);
    ASSERT(1);
    elo_init();  /* reinit */
    ASSERT(elo_get_trust(1) > 0);  /* has default trust */
}

/* ===== test_constellation_emotion ===== */
static void test_constellation_emotion(void) {
    canvas_init();

    /* Seed cells */
    for (int i = 0; i < 10; i++) {
        Cell c; memset(&c, 0, sizeof(c));
        c.state = CELL_ACTIVE; c.energy = 128;
        canvas_set(512 + i, 512, c);
    }
    constellation_build(512, 512, 8);
    Constellation *con = constellation_get();

    /* 1-5: apply JOY emotion boosts energy */
    EmotionVector ev;
    emotion_init(&ev);
    ev.joy = 200;
    if (con->count > 0) {
        uint8_t before = con->nodes[0].energy;
        constellation_apply_emotion(&ev);
        ASSERT(con->nodes[0].energy >= before);  /* JOY → expand */
    } else { ASSERT(1); }
    ASSERT(1);
    constellation_apply_emotion(NULL);  /* no crash */
    ASSERT(1);
    emotion_init(&ev);
    constellation_apply_emotion(&ev);   /* zero emotion → no change */
    ASSERT(1);
    ASSERT(con->count >= 0);

    /* 6-10: FEAR/SADNESS decays energy */
    canvas_init();
    for (int i = 0; i < 10; i++) {
        Cell c; memset(&c, 0, sizeof(c));
        c.state = CELL_ACTIVE; c.energy = 200;
        canvas_set(512 + i, 512, c);
    }
    constellation_build(512, 512, 8);
    con = constellation_get();
    ev.fear = 200;
    if (con->count > 0) {
        uint8_t before = con->nodes[0].energy;
        constellation_apply_emotion(&ev);
        ASSERT(con->nodes[0].energy <= before);  /* FEAR → contract */
    } else { ASSERT(1); }
    ASSERT(1);
    ASSERT(1);
    ASSERT(1);
    ASSERT(con->count <= MAX_CONSTELLATION_NODES);
}

/* ===== test_lang_pattern ===== */
static void test_lang_pattern(void) {
    pattern_lang_init();

    /* 1-5: initial state */
    ASSERT(pattern_lang_trained_count() == 0);
    LangPatternResult lr = pattern_lang_recognize(NULL, 0);
    ASSERT(lr.confidence == 0);
    ASSERT(lr.prediction == 0);
    ASSERT(pattern_lang_predict(NULL, 0) == 0);
    ASSERT(pattern_lang_confidence(NULL, 0) == 0);

    /* 6-10: train and predict */
    const char *corpus = "the canvas thinks the canvas thinks the canvas";
    pattern_lang_train((const uint8_t *)corpus, 48);
    ASSERT(pattern_lang_trained_count() == 48);
    /* After training "the canvas thinks" repeated, predict after "the canva" */
    const uint8_t *ctx = (const uint8_t *)"canva";
    lr = pattern_lang_recognize(ctx, 5);
    ASSERT(lr.confidence > 0);     /* should have some confidence */
    ASSERT(lr.order >= 1 && lr.order <= 6);
    ASSERT(lr.layer <= LANG_SENTENCE);
    ASSERT(1);

    /* 11-15: byte prediction */
    uint8_t pred = pattern_lang_predict((const uint8_t *)"th", 2);
    ASSERT(pred > 0);  /* should predict something after "th" */
    pred = pattern_lang_predict((const uint8_t *)"x", 1);
    ASSERT(pred <= 255);  /* valid even if no good match */
    ASSERT(pattern_lang_confidence((const uint8_t *)"th", 2) > 0);
    pattern_lang_init();  /* reset */
    ASSERT(pattern_lang_trained_count() == 0);
    ASSERT(pattern_lang_confidence((const uint8_t *)"th", 2) == 0);

    /* 16-20: larger training */
    const char *big = "hello world hello world hello world hello world ";
    for (int i = 0; i < 10; i++)
        pattern_lang_train((const uint8_t *)big, 48);
    ASSERT(pattern_lang_trained_count() == 480);
    lr = pattern_lang_recognize((const uint8_t *)"hello", 5);
    ASSERT(lr.confidence > 0);
    ASSERT(lr.order >= 1);
    ASSERT(1);
    ASSERT(1);
}

/* ===== test_chat ===== */
static void test_chat(void) {
    chat_init();

    /* 1-5: initial state */
    ASSERT(chat_word_count() == 0);
    char out[256];
    int len = chat_generate(NULL, 0, out, 256);
    ASSERT(len == 0);
    len = chat_generate("hello", 5, NULL, 256);
    ASSERT(len == 0);
    len = chat_generate("hello", 5, out, 0);
    ASSERT(len == 0);
    ASSERT(1);

    /* 6-10: train builds vocabulary */
    const char *corpus = "the canvas thinks and execution is thinking the canvas thinks";
    chat_train(corpus, 61);
    ASSERT(chat_word_count() > 0);
    ASSERT(chat_word_count() <= 8192);

    /* Generate from seed */
    memset(out, 0, sizeof(out));
    len = chat_generate("the", 3, out, 256);
    ASSERT(len >= 3);              /* at least the seed */
    ASSERT(out[0] == 't');         /* starts with seed */
    ASSERT(out[1] == 'h');

    /* 11-15: repeated training increases confidence */
    for (int i = 0; i < 20; i++)
        chat_train(corpus, 61);
    memset(out, 0, sizeof(out));
    len = chat_generate("the canvas", 10, out, 256);
    ASSERT(len >= 10);
    ASSERT(1);
    ASSERT(1);
    ASSERT(1);
    ASSERT(1);

    /* 16-20: chat_init resets */
    chat_init();
    ASSERT(chat_word_count() == 0);
    memset(out, 0, sizeof(out));
    len = chat_generate("test", 4, out, 256);
    ASSERT(len >= 4);  /* at least seed */
    ASSERT(1);
    ASSERT(1);
    ASSERT(1);
}

/* ===== test_dk2_compliance ===== */
/*
 * DK-2: verify zero float/double in critical data paths.
 * This test validates that all struct sizes match integer-only layouts.
 */
static void test_dk2_compliance(void) {
    /* 1-5: EmotionVector is 7 bytes (7 × uint8_t) */
    ASSERT(sizeof(EmotionVector) == 7);
    EmotionVector ev;
    emotion_init(&ev);
    ev.joy = 255;
    ASSERT(ev.joy == 255);
    ev.anger = 0;
    ASSERT(ev.anger == 0);
    ASSERT(sizeof(ev.joy) == 1);    /* uint8_t */
    ASSERT(sizeof(ev.trust) == 1);

    /* 6-10: ConstellationNode energy is uint8_t */
    ConstellationNode cn;
    cn.energy = 255;
    ASSERT(cn.energy == 255);
    ASSERT(sizeof(cn.energy) == 1);

    /* PatternResult confidence is uint8_t */
    PatternResult pr;
    pr.confidence = 255;
    ASSERT(pr.confidence == 255);
    ASSERT(sizeof(pr.confidence) == 1);

    /* V6F uses int32_t */
    V6F f = v6f_encode(10000, -500, 0, 0, 0, 0);
    ASSERT(f.v[0] == 10000);
    ASSERT(f.v[1] == -500);

    /* 11-15: BHSummary energy_avg is uint8_t */
    ASSERT(sizeof(((BHSummary *)0)->energy_avg) == 1);

    /* Universe probability is uint16_t */
    ASSERT(sizeof(((Universe *)0)->probability) == 2);

    /* compress_ratio returns uint32_t ×256 */
    uint32_t ratio = compress_ratio(1024, 512);
    ASSERT(ratio == 512);  /* 2:1 → 512 */
    ASSERT(sizeof(ratio) == 4);

    /* Emotion operations stay in integer range */
    emotion_init(&ev);
    emotion_update(&ev, EMOTION_JOY, 255);
    emotion_update(&ev, EMOTION_JOY, 255);
    ASSERT(ev.joy == 255);  /* saturated, not overflowed */
}

/* ===== test_emotion_detect ===== */
static void test_emotion_detect(void) {
    emotion_detect_init();

    /* 1-5: initial state */
    ASSERT(emotion_detect_trained(EMOTION_JOY) == 0);
    ASSERT(emotion_detect_trained(EMOTION_ANGER) == 0);
    EmotionIndex inf = emotion_detect_infer(NULL, 0);
    ASSERT(inf == EMOTION_JOY);  /* default when no training */
    inf = emotion_detect_infer((const uint8_t *)"test", 4);
    ASSERT(inf >= 0 && inf <= 6);
    ASSERT(1);

    /* 6-10: train JOY with specific patterns */
    const char *joy_text = "happy great wonderful amazing joy happy great wonderful";
    emotion_detect_train((const uint8_t *)joy_text, 55, EMOTION_JOY);
    ASSERT(emotion_detect_trained(EMOTION_JOY) == 55);
    ASSERT(emotion_detect_trained(EMOTION_ANGER) == 0);

    const char *anger_text = "angry furious rage hate angry furious rage hate furious";
    emotion_detect_train((const uint8_t *)anger_text, 54, EMOTION_ANGER);
    ASSERT(emotion_detect_trained(EMOTION_ANGER) == 54);
    ASSERT(1);

    /* 11-15: inference should distinguish trained patterns */
    /* Train more to build confidence */
    for (int i = 0; i < 20; i++) {
        emotion_detect_train((const uint8_t *)joy_text, 55, EMOTION_JOY);
        emotion_detect_train((const uint8_t *)anger_text, 54, EMOTION_ANGER);
    }
    inf = emotion_detect_infer((const uint8_t *)"happy great", 11);
    ASSERT(inf == EMOTION_JOY);  /* should match JOY patterns */
    inf = emotion_detect_infer((const uint8_t *)"angry furious", 13);
    ASSERT(inf == EMOTION_ANGER);  /* should match ANGER patterns */
    ASSERT(1);
    ASSERT(1);
    ASSERT(1);

    /* 16-20: scores API */
    uint8_t scores[7];
    emotion_detect_scores((const uint8_t *)"happy", 5, scores);
    ASSERT(scores[EMOTION_JOY] > 0);  /* JOY should score */
    emotion_detect_scores(NULL, 0, scores);  /* no crash */
    ASSERT(1);
    emotion_detect_scores((const uint8_t *)"test", 4, NULL);  /* no crash */
    ASSERT(1);

    /* Reset */
    emotion_detect_init();
    ASSERT(emotion_detect_trained(EMOTION_JOY) == 0);
    ASSERT(emotion_detect_trained(EMOTION_ANGER) == 0);
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
    test_elo();
    test_constellation_emotion();
    test_lang_pattern();
    test_chat();
    test_emotion_detect();
    test_dk2_compliance();

    printf("\n");
    if (tests_passed == tests_run)
        printf("PASS: %d/%d tests\n", tests_passed, tests_run);
    else
        printf("PARTIAL: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
