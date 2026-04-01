#ifndef CELL_H
#define CELL_H

#include "canvasos.h"

/* Extended cell metadata for pattern/constellation use */

#define MAX_CONSTELLATION_NODES 64
#define MAX_PATTERN_CACHE       128

/* Pattern training entry — confidence 0-255 (DK-2) */
typedef struct {
    int          x, y;
    PatternLayer layer;
    uint16_t     id;
    uint8_t      confidence;
} PatternEntry;

/* Constellation graph */
typedef struct {
    ConstellationNode nodes[MAX_CONSTELLATION_NODES];
    int               count;
    int               center_x, center_y;
} Constellation;

/* BH summary — energy_avg 0-255 (DK-2) */
typedef struct {
    int      summary_id;
    uint64_t from_time;
    uint64_t to_time;
    uint8_t  energy_avg;
    int      record_count;
} BHSummary;

/* WH record */
typedef struct {
    int      record_id;
    uint64_t timestamp;
    uint32_t canvas_snapshot_id;
    uint8_t  metadata[64];
    size_t   data_size;
} WHRecord;

/* Lane task function type - defined in canvasos.h as TaskFn */

/* Lane structure */
typedef struct {
    uint8_t  id;
    bool     active;
    uint8_t  priority;
    TaskFn   task_fn;
    void    *context;
    uint64_t ticks;
} Lane;

/* Branch patch (COW delta) */
typedef struct {
    int  x, y;
    Cell cell;
} Patch;

#define MAX_PATCHES 1024

/* Branch structure */
typedef struct {
    int     id;
    int     parent_id;
    uint64_t diverge_tick;
    Patch   patches[MAX_PATCHES];
    int     patch_count;
    bool    active;
} Branch;

/* Universe structure — probability 0-65535 (DK-2, 65535 = 1.0) */
typedef struct {
    int      id;
    int      branch_id;
    uint16_t probability;
    bool     active;
} Universe;

/* Process structure */
typedef struct {
    int          id;
    ProcessState state;
    uint8_t      priority;
    int          lane_id;
    char         name[32];
    TaskFn       task_fn;
} Process;

/* FS node */
#define MAX_FS_DATA (4096)

typedef struct {
    char     name[64];
    uint8_t  data[MAX_FS_DATA];
    size_t   size;
    uint64_t created_at;
    uint64_t modified_at;
    bool     used;
} FSNode;

/* These return pointers — declared here after struct defs */
WHRecord      *wh_get(int record_id);
WHRecord      *wh_latest(void);
BHSummary     *bh_get_summary(int summary_id);
Constellation *constellation_get(void);

/* Lane getter */
Lane      *lane_get(int lane_id);

/* Universe getter */
Universe  *multiverse_get(int universe_id);

#endif /* CELL_H */
