/*
 * compress.c — A/O Pipeline Compression Executor + ELO Cascade
 *
 * Pipeline:
 *   input → ELO trust-weighted n-gram prediction → canvas energy mapping
 *         → correction collection (WH) → history compression (BH)
 *         → stream metadata → format-optimized output
 *
 * ELO integration:
 *   Layer 3 (100x) trust → weights n-gram orders 1-2
 *   Layer 2 (10x)  trust → weights n-gram orders 3-4
 *   Layer 1 (1x)   trust → weights n-gram orders 5-6
 *
 *   Cascade: fast layer accuracy → slow layer trust boost.
 *   "The second hand's pattern becomes the minute hand's value."
 *
 * Output format (auto-selected):
 *   FORMAT_CORRECTIONS (0x01): [4B size][1B tag][4B count][corrections...]
 *   FORMAT_XOR_DELTA   (0x00): [4B size][1B tag][XOR deltas...]
 */

#include "canvasos.h"
#include "cell.h"
#include <string.h>

/* ===== N-gram Prediction Engine (6-order, FNV-1a hash) ===== */

#define NGRAM_HASH_SIZE  65536
#define NGRAM_MAX_ORDER  6
#define FORMAT_XOR_DELTA    0x00
#define FORMAT_CORRECTIONS  0x01

/* Per-order hash tables: predicted byte and confidence count */
static uint8_t  ngram_pred[NGRAM_MAX_ORDER][NGRAM_HASH_SIZE];
static uint16_t ngram_conf[NGRAM_MAX_ORDER][NGRAM_HASH_SIZE];

static uint32_t fnv1a(const uint8_t *data, int len) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h & (NGRAM_HASH_SIZE - 1);
}

static void ngram_reset(void) {
    memset(ngram_pred, 0, sizeof(ngram_pred));
    memset(ngram_conf, 0, sizeof(ngram_conf));
}

/*
 * Train: update all applicable order tables.
 * Correct prediction → confidence++.
 * Wrong prediction → confidence-- (replace when exhausted).
 */
static void ngram_train(const uint8_t *ctx, int ctx_len, uint8_t actual) {
    for (int order = 1; order <= NGRAM_MAX_ORDER; order++) {
        if (ctx_len < order) break;
        uint32_t h = fnv1a(ctx + ctx_len - order, order);

        if (ngram_pred[order - 1][h] == actual) {
            if (ngram_conf[order - 1][h] < 65535)
                ngram_conf[order - 1][h]++;
        } else {
            if (ngram_conf[order - 1][h] > 1) {
                ngram_conf[order - 1][h]--;
            } else {
                ngram_pred[order - 1][h] = actual;
                ngram_conf[order - 1][h] = 1;
            }
        }
    }
}

/* ===== Combined Prediction: N-gram + ELO Trust Cascade ===== */

/*
 * ELO trust scores modulate n-gram order weights:
 *   - Layer 3 trust → orders 1-2 (local byte patterns)
 *   - Layer 2 trust → orders 3-4 (phrase patterns)
 *   - Layer 1 trust → orders 5-6 (document patterns)
 *
 * weight = trust[layer] * ngram_conf[order]
 *
 * When cascade detects fast layer is accurate, it boosts slow layer trust.
 * This adapts prediction to the actual data structure — no static bias.
 */
static uint8_t combined_predict(const uint8_t *ctx, int ctx_len) {
    uint8_t trust[3];
    trust[0] = elo_get_trust(3);   /* orders 1-2 */
    trust[1] = elo_get_trust(2);   /* orders 3-4 */
    trust[2] = elo_get_trust(1);   /* orders 5-6 */

    uint8_t  best   = 0;
    uint32_t best_w = 0;

    for (int order = 1; order <= NGRAM_MAX_ORDER; order++) {
        if (ctx_len < order) break;

        int layer_idx = (order - 1) / 2;  /* 0,0,1,1,2,2 */

        uint32_t h = fnv1a(ctx + ctx_len - order, order);
        /*
         * weight = order * trust * conf
         *
         * Baseline: order gives static preference to longer contexts.
         * Cascade:  trust adapts dynamically based on layer accuracy.
         *
         * At initial trust=128 for all: weight ∝ order*conf (same as static).
         * After cascade: trust diverges, reweighting order importance.
         * e.g. L3 trust=200 → short context boosted
         *      L1 trust=100 → long context dampened
         */
        uint32_t w = (uint32_t)order * trust[layer_idx] * ngram_conf[order - 1][h];
        if (w > best_w) {
            best_w = w;
            best   = ngram_pred[order - 1][h];
        }
    }

    return best;
}

/* Train both n-gram and ELO cascade */
static void combined_train(const uint8_t *ctx, int ctx_len, uint8_t actual) {
    ngram_train(ctx, ctx_len, actual);
    elo_feed(ctx, ctx_len, actual);
}

/* Reset both prediction engines */
static void combined_reset(void) {
    ngram_reset();
    elo_init();
}

/* Map byte context to canvas cell: energy += 1, G channel = last byte */
static void canvas_feed_byte(const uint8_t *ctx, int ctx_len, uint8_t actual) {
    if (ctx_len < 1) return;
    uint32_t h = fnv1a(ctx + ctx_len - 1, 1);
    int cx = (int)(h & 0xFFF);
    int cy = (int)((h >> 12) & 0xFFF);

    Cell c = canvas_get(cx, cy);
    c.color.g = actual;
    c.state   = CELL_ACTIVE;
    if (c.energy < 255) c.energy++;
    canvas_set(cx, cy, c);
}

/* Push byte into sliding context window */
static void ctx_push(uint8_t *ctx, int *ctx_len, uint8_t byte) {
    if (*ctx_len < NGRAM_MAX_ORDER) {
        ctx[(*ctx_len)++] = byte;
    } else {
        memmove(ctx, ctx + 1, NGRAM_MAX_ORDER - 1);
        ctx[NGRAM_MAX_ORDER - 1] = byte;
    }
}

/* ===== Compression: A/O Pipeline Executor ===== */

int compress_predicted_delta(const uint8_t *input, size_t input_size,
                              uint8_t *output, size_t output_size) {
    if (!input || !output) return -1;

    /* XOR delta worst case: 5-byte header + input_size deltas */
    if (output_size < input_size + 5) return -1;

    /* 1. Reset A/O pipeline state */
    combined_reset();
    wh_init();

    uint8_t ctx[NGRAM_MAX_ORDER];
    memset(ctx, 0, sizeof(ctx));
    int ctx_len = 0;

    /* 2. Prediction pass: predict → compare → learn → track
     *    Store XOR deltas in output buffer (used if XOR format selected)
     *    Count corrections to decide output format */
    uint8_t *deltas = output + 5;
    uint32_t correction_count = 0;

    for (size_t i = 0; i < input_size; i++) {
        uint8_t predicted = combined_predict(ctx, ctx_len);
        uint8_t actual    = input[i];

        deltas[i] = actual ^ predicted;

        if (predicted != actual) {
            correction_count++;
            /* WH: record correction event */
            uint8_t wh_meta[5];
            wh_meta[0] = (uint8_t)((i >> 16) & 0xFF);
            wh_meta[1] = (uint8_t)((i >> 8)  & 0xFF);
            wh_meta[2] = (uint8_t)(i & 0xFF);
            wh_meta[3] = actual;
            wh_meta[4] = predicted;
            wh_record((uint64_t)i, wh_meta, 5);
        }

        /* Train n-gram + ELO cascade */
        combined_train(ctx, ctx_len, actual);

        /* Feed canvas: energy tracking on byte-mapped cells */
        canvas_feed_byte(ctx, ctx_len, actual);

        /* Advance context window */
        ctx_push(ctx, &ctx_len, actual);
    }

    /* 3. BH: compress correction history patterns */
    if (correction_count > 0) {
        bh_compress_history(0, (uint64_t)input_size);
    }

    /* 4. Stream: record compression metadata as keyframe */
    {
        uint8_t meta[16];
        memset(meta, 0, sizeof(meta));
        meta[0] = (uint8_t)(input_size >> 24);
        meta[1] = (uint8_t)(input_size >> 16);
        meta[2] = (uint8_t)(input_size >> 8);
        meta[3] = (uint8_t)(input_size);
        meta[4] = (uint8_t)(correction_count >> 24);
        meta[5] = (uint8_t)(correction_count >> 16);
        meta[6] = (uint8_t)(correction_count >> 8);
        meta[7] = (uint8_t)(correction_count);
        stream_write_keyframe(meta, 16);
    }

    /* 5. Choose output format: corrections if compact, XOR delta otherwise */
    size_t corr_out = 9 + (size_t)correction_count * 4;
    size_t xor_out  = 5 + input_size;

    if (corr_out < xor_out && corr_out <= output_size) {
        /*
         * Corrections format: only mispredicted bytes are stored.
         * Re-run identical prediction to emit corrections deterministically.
         */
        combined_reset();
        memset(ctx, 0, sizeof(ctx));
        ctx_len = 0;

        size_t pos = 0;

        /* Header: original size (4B big-endian) */
        output[pos++] = (uint8_t)(input_size >> 24);
        output[pos++] = (uint8_t)(input_size >> 16);
        output[pos++] = (uint8_t)(input_size >> 8);
        output[pos++] = (uint8_t)(input_size);

        /* Format tag */
        output[pos++] = FORMAT_CORRECTIONS;

        /* Correction count (4B big-endian) */
        output[pos++] = (uint8_t)(correction_count >> 24);
        output[pos++] = (uint8_t)(correction_count >> 16);
        output[pos++] = (uint8_t)(correction_count >> 8);
        output[pos++] = (uint8_t)(correction_count);

        /* Emit corrections: [3B position][1B actual_value] */
        for (size_t i = 0; i < input_size; i++) {
            uint8_t predicted = combined_predict(ctx, ctx_len);
            uint8_t actual    = input[i];

            if (predicted != actual) {
                output[pos++] = (uint8_t)((i >> 16) & 0xFF);
                output[pos++] = (uint8_t)((i >> 8)  & 0xFF);
                output[pos++] = (uint8_t)(i & 0xFF);
                output[pos++] = actual;
            }

            combined_train(ctx, ctx_len, actual);
            ctx_push(ctx, &ctx_len, actual);
        }

        return (int)pos;
    }

    /* XOR delta format: deltas already at output+5, write header */
    output[0] = (uint8_t)(input_size >> 24);
    output[1] = (uint8_t)(input_size >> 16);
    output[2] = (uint8_t)(input_size >> 8);
    output[3] = (uint8_t)(input_size);
    output[4] = FORMAT_XOR_DELTA;

    return (int)xor_out;
}

int compress_decompress(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size) {
    if (!input || !output || input_size < 5) return -1;

    size_t orig_size = ((size_t)input[0] << 24) | ((size_t)input[1] << 16) |
                       ((size_t)input[2] << 8)  |  (size_t)input[3];
    if (orig_size > output_size) return -1;

    uint8_t format = input[4];

    combined_reset();
    uint8_t ctx[NGRAM_MAX_ORDER];
    memset(ctx, 0, sizeof(ctx));
    int ctx_len = 0;

    if (format == FORMAT_XOR_DELTA) {
        /* XOR delta: replay trust-weighted prediction, XOR with deltas */
        size_t out_pos = 0;
        for (size_t i = 5; i < input_size && out_pos < orig_size; i++) {
            uint8_t predicted = combined_predict(ctx, ctx_len);
            uint8_t byte      = input[i] ^ predicted;
            output[out_pos++] = byte;

            combined_train(ctx, ctx_len, byte);
            ctx_push(ctx, &ctx_len, byte);
        }
        return (int)out_pos;

    } else if (format == FORMAT_CORRECTIONS) {
        /* Corrections: replay prediction, apply corrections at marked positions */
        if (input_size < 9) return -1;

        uint32_t corr_count = ((uint32_t)input[5] << 24) |
                              ((uint32_t)input[6] << 16) |
                              ((uint32_t)input[7] << 8)  |
                               (uint32_t)input[8];

        size_t   corr_off = 9;
        uint32_t corr_idx = 0;

        uint32_t next_pos = 0xFFFFFFFF;
        uint8_t  next_val = 0;
        if (corr_idx < corr_count && corr_off + 4 <= input_size) {
            next_pos = ((uint32_t)input[corr_off]     << 16) |
                       ((uint32_t)input[corr_off + 1] << 8)  |
                        (uint32_t)input[corr_off + 2];
            next_val = input[corr_off + 3];
        }

        size_t out_pos = 0;
        for (size_t i = 0; i < orig_size; i++) {
            uint8_t predicted = combined_predict(ctx, ctx_len);
            uint8_t actual;

            if ((uint32_t)i == next_pos) {
                actual = next_val;
                corr_idx++;
                corr_off += 4;
                if (corr_idx < corr_count && corr_off + 4 <= input_size) {
                    next_pos = ((uint32_t)input[corr_off]     << 16) |
                               ((uint32_t)input[corr_off + 1] << 8)  |
                                (uint32_t)input[corr_off + 2];
                    next_val = input[corr_off + 3];
                } else {
                    next_pos = 0xFFFFFFFF;
                }
            } else {
                actual = predicted;
            }

            output[out_pos++] = actual;
            combined_train(ctx, ctx_len, actual);
            ctx_push(ctx, &ctx_len, actual);
        }
        return (int)out_pos;
    }

    return -1; /* Unknown format */
}

/*
 * Compression ratio × 256 (fixed-point).
 * 256 = 1:1, 512 = 2:1, 1280 = 5:1, etc.
 */
uint32_t compress_ratio(size_t original_size, size_t compressed_size) {
    if (compressed_size == 0) return 0;
    return (uint32_t)((original_size * 256) / compressed_size);
}
