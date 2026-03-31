#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

static uint8_t predict_next_byte(const uint8_t *ctx, int ctx_len) {
    if (ctx_len == 0) return 0;
    /* Simple adaptive prediction: weighted average of context bytes */
    uint32_t sum = 0, weight = 1, total_w = 0;
    for (int i = ctx_len - 1; i >= 0; i--) {
        sum     += ctx[i] * weight;
        total_w += weight;
        weight++;
    }
    return (uint8_t)(sum / total_w);
}

int compress_predicted_delta(const uint8_t *input, size_t input_size,
                              uint8_t *output, size_t output_size) {
    if (!input || !output || output_size < input_size + 4) return -1;

    uint8_t ctx[COMPRESS_CONTEXT_LEN];
    memset(ctx, 0, sizeof(ctx));
    size_t out_pos = 0;

    /* Write original size header (4 bytes) */
    output[out_pos++] = (uint8_t)(input_size >> 24);
    output[out_pos++] = (uint8_t)(input_size >> 16);
    output[out_pos++] = (uint8_t)(input_size >> 8);
    output[out_pos++] = (uint8_t)(input_size);

    for (size_t i = 0; i < input_size && out_pos < output_size; i++) {
        uint8_t predicted = predict_next_byte(ctx, COMPRESS_CONTEXT_LEN);
        uint8_t delta     = input[i] ^ predicted;
        output[out_pos++] = delta;

        /* Shift context */
        memmove(ctx, ctx + 1, COMPRESS_CONTEXT_LEN - 1);
        ctx[COMPRESS_CONTEXT_LEN - 1] = input[i];
    }
    return (int)out_pos;
}

int compress_decompress(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size) {
    if (!input || !output || input_size < 4) return -1;

    size_t orig_size = ((size_t)input[0] << 24) | ((size_t)input[1] << 16) |
                       ((size_t)input[2] << 8)  |  (size_t)input[3];
    if (orig_size > output_size) return -1;

    uint8_t ctx[COMPRESS_CONTEXT_LEN];
    memset(ctx, 0, sizeof(ctx));
    size_t out_pos = 0;

    for (size_t i = 4; i < input_size && out_pos < orig_size; i++) {
        uint8_t predicted  = predict_next_byte(ctx, COMPRESS_CONTEXT_LEN);
        uint8_t byte       = input[i] ^ predicted;
        output[out_pos++]  = byte;

        memmove(ctx, ctx + 1, COMPRESS_CONTEXT_LEN - 1);
        ctx[COMPRESS_CONTEXT_LEN - 1] = byte;
    }
    return (int)out_pos;
}

float compress_ratio(size_t original_size, size_t compressed_size) {
    if (compressed_size == 0) return 0.0f;
    return (float)original_size / (float)compressed_size;
}
