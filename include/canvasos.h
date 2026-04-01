#ifndef CANVASOS_H
#define CANVASOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* A/O = AI/OS: The canvas thinks, and execution IS thinking */

#define AO_VERSION      "1.0.0"
#define CANVAS_WIDTH    4096
#define CANVAS_HEIGHT   4096
#define LANE_COUNT      256
#define UNIVERSE_COUNT  16
#define KEYFRAME_INTERVAL 16
#define COMPRESS_CONTEXT_LEN 8
#define MAX_FILES       1024

/* ABGR pixel/color type */
typedef struct {
    uint8_t a;
    uint8_t b;
    uint8_t g;
    uint8_t r;
} ABGR;

/* Cell channel indices */
typedef enum {
    CH_A = 0,
    CH_B = 1,
    CH_G = 2,
    CH_R = 3
} CellChannel;

/* Cell state flags */
typedef enum {
    CELL_IDLE   = 0,
    CELL_ACTIVE = 1,
    CELL_LOCKED = 2,
    CELL_DEAD   = 3
} CellState;

/* Core cell structure */
typedef struct {
    ABGR     color;
    uint8_t  energy;
    uint8_t  state;
    uint16_t id;
} Cell;

/* Pattern recognition layers */
typedef enum {
    LAYER_RAW      = 0,
    LAYER_EDGE     = 1,
    LAYER_SHAPE    = 2,
    LAYER_OBJECT   = 3,
    LAYER_CONTEXT  = 4,
    LAYER_ABSTRACT = 5
} PatternLayer;

/* Emotion indices (Plutchik) */
typedef enum {
    EMOTION_JOY      = 0,
    EMOTION_TRUST    = 1,
    EMOTION_FEAR     = 2,
    EMOTION_SURPRISE = 3,
    EMOTION_SADNESS  = 4,
    EMOTION_DISGUST  = 5,
    EMOTION_ANGER    = 6
} EmotionIndex;

/* 7D emotion vector — integer-only (DK-2), 0-255 scale */
typedef struct {
    uint8_t joy;
    uint8_t trust;
    uint8_t fear;
    uint8_t surprise;
    uint8_t sadness;
    uint8_t disgust;
    uint8_t anger;
} EmotionVector;

/* Constellation node — energy 0-255 (DK-2) */
typedef struct {
    int     x, y;
    uint8_t energy;
    int     connections[8];
    int     conn_count;
} ConstellationNode;

/* Pattern recognition result — confidence 0-255 (DK-2) */
typedef struct {
    PatternLayer layer;
    uint8_t      confidence;
    uint16_t     matched_id;
} PatternResult;

/* V6F: 6-element integer financial vector (DK-2, fixed-point ×100) */
typedef struct {
    int32_t v[6];
} V6F;

/* Frame types for stream */
typedef enum {
    FRAME_KEY   = 0,
    FRAME_DELTA = 1
} FrameType;

/* Stream frame */
typedef struct {
    FrameType type;
    uint64_t  timestamp;
    uint8_t   data[1024];
    size_t    data_size;
    uint32_t  checksum;
    int       prev_id;
} Frame;

/* Merge policies */
typedef enum {
    MERGE_LATEST  = 0,
    MERGE_OLDEST  = 1,
    MERGE_AND     = 2,
    MERGE_OR      = 3,
    MERGE_XOR     = 4,
    MERGE_AVERAGE = 5
} MergePolicy;

/* Process states */
typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_BLOCKED = 2,
    PROC_ZOMBIE  = 3
} ProcessState;

/* Task function pointer type (used by Lane, Scheduler) */
typedef void (*TaskFn)(void *ctx);

/* Scan callback type */
typedef void (*ScanCallback)(int x, int y, Cell *c);

/* ===== canvas.c ===== */
void  canvas_init(void);
Cell  canvas_get(int x, int y);
void  canvas_set(int x, int y, Cell c);
void  canvas_tick(void);
void  canvas_clear(void);

/* ===== gate.c ===== */
void  gate_init(void);
bool  gate_lock(int tile_x, int tile_y, uint16_t owner);
bool  gate_unlock(int tile_x, int tile_y, uint16_t owner);
bool  gate_can_access(int tile_x, int tile_y, uint16_t requester);
void  gate_isolate_region(int x0, int y0, int x1, int y1, uint16_t owner);

/* ===== scan.c ===== */
void  scan_ring(int cx, int cy, int radius, ScanCallback cb);
void  scan_spiral(int cx, int cy, int max_radius, ScanCallback cb);
int   scan_find_pattern(int cx, int cy, int max_r, uint8_t target_energy);
int   scan_count_active(int cx, int cy, int radius);

/* ===== pattern.c ===== */
PatternResult pattern_recognize(int x, int y, int radius);
void          pattern_layer_raw(int x, int y);
void          pattern_layer_edge(int x, int y, int radius);
void          pattern_layer_shape(int x, int y, int radius);
void          pattern_layer_object(int x, int y, int radius);
void          pattern_layer_context(int x, int y, int radius);
void          pattern_layer_abstract(int x, int y, int radius);
void          pattern_train(int x, int y, PatternLayer layer, uint16_t id);

/* ===== constellation.c ===== */
void  constellation_build(int cx, int cy, int radius);
void  constellation_propagate(int steps);
int   constellation_infer(uint8_t query_energy);
void  constellation_update(int x, int y, int16_t delta);

/* ===== emotion.c ===== */
void          emotion_init(EmotionVector *ev);
void          emotion_update(EmotionVector *ev, EmotionIndex stimulus, uint8_t intensity);
EmotionVector emotion_blend(EmotionVector a, EmotionVector b, uint8_t ratio);
EmotionIndex  emotion_dominant(EmotionVector *ev);
uint8_t       emotion_to_energy(EmotionVector *ev);

/* ===== stream.c ===== */
int    stream_write_keyframe(const uint8_t *data, size_t size);
int    stream_write_delta(int prev_id, const uint8_t *data, size_t size);
Frame *stream_read(int frame_id);
int    stream_reconstruct(int frame_id, uint8_t *out, size_t out_size);
int    stream_frame_count(void);
void   stream_reset(void);

/* ===== elo.c ===== */
void    elo_init(void);
uint8_t elo_predict(const uint8_t *ctx, int ctx_len);
uint8_t elo_confidence(const uint8_t *ctx, int ctx_len);
void    elo_feed(const uint8_t *ctx, int ctx_len, uint8_t actual);
uint8_t elo_get_trust(int layer);

/* ===== compress.c ===== */
int      compress_predicted_delta(const uint8_t *input, size_t input_size,
                                   uint8_t *output, size_t output_size);
int      compress_decompress(const uint8_t *input, size_t input_size,
                              uint8_t *output, size_t output_size);
uint32_t compress_ratio(size_t original_size, size_t compressed_size);

/* ===== v6f.c ===== */
V6F      v6f_encode(int32_t price, int32_t open, int32_t high, int32_t low,
                    int32_t volume, int32_t ts);
void     v6f_decode(const V6F *f, int32_t *price, int32_t *open, int32_t *high,
                    int32_t *low, int32_t *volume, int32_t *ts);
uint32_t v6f_distance_sq(const V6F *a, const V6F *b);
int32_t  v6f_similarity(const V6F *a, const V6F *b);

/* ===== wh.c ===== */
/* WHRecord type defined in cell.h */
void      wh_init(void);
int       wh_record(uint64_t timestamp, const uint8_t *data, size_t size);
int       wh_count(void);

/* ===== bh.c ===== */
/* BHSummary type defined in cell.h */
void      bh_init(void);
int       bh_compress_history(uint64_t from_time, uint64_t to_time);
void      bh_forget(int record_id);
int       bh_summarize_pattern(const int *pattern_ids, int count);

/* ===== merge.c ===== */
Cell  merge_cells(Cell a, Cell b, MergePolicy policy);
void  merge_region(int x0, int y0, int x1, int y1, MergePolicy policy);
Cell  merge_apply_delta(Cell base, Cell delta);
Cell  merge_conflict_resolve(const Cell *candidates, int count, MergePolicy policy);

/* ===== lane.c ===== */
void  lane_init(void);
int   lane_spawn(TaskFn task_fn, void *context, uint8_t priority);
void  lane_kill(int lane_id);
void  lane_tick(void);
int   lane_count_active(void);

/* ===== branch.c ===== */
void  branch_system_init(void);
int   branch_create(int parent_id);
void  branch_apply_patch(int branch_id, int x, int y, Cell cell);
void  branch_merge(int src_branch_id, int dst_branch_id);
Cell  branch_get_cell(int branch_id, int x, int y);
void  branch_delete(int branch_id);
int   branch_count_active(void);

/* ===== multiverse.c ===== */
void  multiverse_init(void);
int   multiverse_spawn(int parent_universe, int branch_at_tick);
void  multiverse_collapse(int universe_id);
Cell  multiverse_get_cell(int universe_id, int x, int y);
void  multiverse_probability_update(int universe_id, uint16_t evidence_weight);
int   multiverse_active_count(void);

/* ===== scheduler.c ===== */
void  scheduler_init(void);
int   scheduler_spawn(const char *name, uint8_t priority, TaskFn task_fn);
void  scheduler_tick(void);
void  scheduler_kill(int process_id);
void  scheduler_list(void);
int   scheduler_active_count(void);

/* ===== fs.c ===== */
void  fs_init(void);
int   fs_write(const char *path, const uint8_t *data, size_t size);
int   fs_read(const char *path, uint8_t *buffer, size_t buf_size);
void  fs_delete(const char *path);
void  fs_list(const char *dir);
bool  fs_exists(const char *path);

#endif /* CANVASOS_H */
