/* Legacy: hash n-gram model. For reference only. Not part of A/O core. */

#include <stdint.h>
#include <string.h>

#define BT_HASH_SIZE 65536
#define BT_ORDER     4

static uint8_t  bt_next[BT_HASH_SIZE];
static uint32_t bt_counts[BT_HASH_SIZE];

static uint32_t bt_hash(const uint8_t *ctx, int len) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= ctx[i];
        h *= 16777619u;
    }
    return h % BT_HASH_SIZE;
}

void bt_model_init(void) {
    memset(bt_next,   0, sizeof(bt_next));
    memset(bt_counts, 0, sizeof(bt_counts));
}

uint8_t bt_model_predict(const uint8_t *context, int len) {
    if (len > BT_ORDER) { context += len - BT_ORDER; len = BT_ORDER; }
    uint32_t h = bt_hash(context, len);
    return bt_next[h];
}

void bt_model_train(const uint8_t *context, int len, uint8_t next_byte) {
    if (len > BT_ORDER) { context += len - BT_ORDER; len = BT_ORDER; }
    uint32_t h = bt_hash(context, len);
    if (bt_counts[h] == 0 || next_byte == bt_next[h]) {
        bt_next[h] = next_byte;
        bt_counts[h]++;
    } else if (bt_counts[h] > 0) {
        /* Simple majority: replace if new byte seen more often (not tracked here) */
        bt_next[h]   = next_byte;
        bt_counts[h] = 1;
    }
}
