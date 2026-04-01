/*
 * chat.c — 2-stage text generation engine, integer-only (DK-2)
 *
 * Stage 1: Word-level n-gram prediction (order 1-4 word contexts)
 *   - Maintains a word-level hash table for next-word prediction
 *   - Tokens delimited by spaces
 *
 * Stage 2: Byte-level fallback (order 1-6 byte n-gram)
 *   - Uses pattern_lang engine when word prediction fails
 *   - Generates character-by-character
 *
 * Training: feed corpus text → builds both word and byte models
 * Generation: seed context → word predict → byte fallback → output
 */

#include "canvasos.h"
#include "cell.h"
#include <string.h>

/* ===== Word-level n-gram engine ===== */

#define WORD_HASH_SIZE  16384
#define WORD_MAX_ORDER  4
#define WORD_MAX_LEN    64
#define WORD_VOCAB_SIZE 8192

/* Word vocabulary: index → string mapping */
static char    word_vocab[WORD_VOCAB_SIZE][WORD_MAX_LEN];
static int     word_vocab_count = 0;

/* Word n-gram tables: context hash → predicted word index + confidence */
static uint16_t word_pred[WORD_MAX_ORDER][WORD_HASH_SIZE];
static uint16_t word_conf[WORD_MAX_ORDER][WORD_HASH_SIZE];

static int chat_ready = 0;

static uint32_t word_hash(const uint16_t *ids, int count) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < count; i++) {
        h ^= (ids[i] & 0xFF);
        h *= 16777619u;
        h ^= (ids[i] >> 8);
        h *= 16777619u;
    }
    return h & (WORD_HASH_SIZE - 1);
}

/* Find or add word to vocabulary, returns index */
static uint16_t word_to_id(const char *word, int len) {
    if (len <= 0 || len >= WORD_MAX_LEN) len = WORD_MAX_LEN - 1;

    /* Search existing */
    for (int i = 0; i < word_vocab_count; i++) {
        int match = 1;
        for (int j = 0; j < len; j++) {
            if (word_vocab[i][j] != word[j]) { match = 0; break; }
        }
        if (match && word_vocab[i][len] == '\0') return (uint16_t)i;
    }

    /* Add new */
    if (word_vocab_count >= WORD_VOCAB_SIZE) return 0;
    int idx = word_vocab_count++;
    for (int j = 0; j < len && j < WORD_MAX_LEN - 1; j++)
        word_vocab[idx][j] = word[j];
    word_vocab[idx][len < WORD_MAX_LEN ? len : WORD_MAX_LEN - 1] = '\0';
    return (uint16_t)idx;
}

void chat_init(void) {
    memset(word_vocab, 0, sizeof(word_vocab));
    memset(word_pred, 0, sizeof(word_pred));
    memset(word_conf, 0, sizeof(word_conf));
    word_vocab_count = 0;
    pattern_lang_init();
    chat_ready = 1;
}

/* Tokenize text into word IDs. Returns count. */
static int tokenize(const char *text, int text_len, uint16_t *ids, int max_ids) {
    int count = 0;
    int i = 0;
    while (i < text_len && count < max_ids) {
        /* Skip spaces */
        while (i < text_len && text[i] == ' ') i++;
        if (i >= text_len) break;

        /* Find word end */
        int start = i;
        while (i < text_len && text[i] != ' ') i++;
        int wlen = i - start;
        if (wlen > 0) {
            ids[count++] = word_to_id(text + start, wlen);
        }
    }
    return count;
}

void chat_train(const char *text, int text_len) {
    if (!chat_ready) chat_init();
    if (!text || text_len <= 0) return;

    /* Stage 1: Train word n-grams */
    uint16_t ids[256];
    int wcount = tokenize(text, text_len, ids, 256);

    for (int i = 0; i < wcount; i++) {
        for (int order = 1; order <= WORD_MAX_ORDER; order++) {
            if (i < order) continue;
            uint32_t h = word_hash(ids + i - order, order);
            if (word_pred[order - 1][h] == ids[i]) {
                if (word_conf[order - 1][h] < 65535)
                    word_conf[order - 1][h]++;
            } else if (word_conf[order - 1][h] == 0) {
                word_pred[order - 1][h] = ids[i];
                word_conf[order - 1][h] = 1;
            }
        }
    }

    /* Stage 2: Train byte n-grams */
    pattern_lang_train((const uint8_t *)text, (size_t)text_len);
}

/*
 * Generate text into output buffer.
 *
 * Algorithm:
 *   1. Tokenize seed into word IDs
 *   2. For each output word:
 *      a. Try word n-gram prediction (order 4→1, best confidence wins)
 *      b. If confidence > threshold → append predicted word
 *      c. If word prediction fails → byte-level generation fallback
 *   3. Stop at max_len or sentence-ending punctuation
 *
 * Returns bytes written.
 */
int chat_generate(const char *seed, int seed_len, char *output, int max_len) {
    if (!chat_ready) chat_init();
    if (!seed || seed_len <= 0 || !output || max_len <= 0) return 0;

    /* Copy seed to output as starting context */
    int out_pos = 0;
    int copy_len = seed_len < max_len - 1 ? seed_len : max_len - 1;
    memcpy(output, seed, (size_t)copy_len);
    out_pos = copy_len;

    /* Tokenize current context for word prediction */
    uint16_t ctx_ids[64];
    int ctx_count = tokenize(output, out_pos, ctx_ids, 64);

    int words_generated = 0;
    int max_words = 20;  /* limit output length */

    while (words_generated < max_words && out_pos < max_len - WORD_MAX_LEN - 2) {
        /* === Stage 1: Word n-gram prediction === */
        uint16_t best_word = 0;
        uint32_t best_conf = 0;

        for (int order = WORD_MAX_ORDER; order >= 1; order--) {
            if (ctx_count < order) continue;
            uint32_t h = word_hash(ctx_ids + ctx_count - order, order);
            uint32_t w = (uint32_t)order * word_conf[order - 1][h];
            if (w > best_conf && word_conf[order - 1][h] > 0) {
                best_conf = w;
                best_word = word_pred[order - 1][h];
            }
        }

        if (best_conf > 2 && best_word < (uint16_t)word_vocab_count) {
            /* Word prediction succeeded — append word */
            const char *w = word_vocab[best_word];
            int wlen = 0;
            while (w[wlen]) wlen++;

            if (out_pos + wlen + 1 >= max_len) break;
            output[out_pos++] = ' ';
            memcpy(output + out_pos, w, (size_t)wlen);
            out_pos += wlen;

            /* Update context */
            if (ctx_count < 64) ctx_ids[ctx_count++] = best_word;
            else {
                /* Shift left */
                for (int i = 0; i < 63; i++) ctx_ids[i] = ctx_ids[i + 1];
                ctx_ids[63] = best_word;
            }
            words_generated++;
        } else {
            /* === Stage 2: Byte-level fallback === */
            /* Generate up to one word via byte prediction */
            if (out_pos + 1 < max_len) output[out_pos++] = ' ';

            int byte_start = out_pos;
            for (int b = 0; b < WORD_MAX_LEN - 1 && out_pos < max_len - 1; b++) {
                int ctx_len = out_pos > 6 ? 6 : out_pos;
                uint8_t pred = pattern_lang_predict(
                    (const uint8_t *)output + out_pos - ctx_len, ctx_len);
                if (pred == 0 || pred == ' ') break;
                output[out_pos++] = (char)pred;
            }

            /* If no bytes generated, stop */
            if (out_pos == byte_start) break;

            /* Register generated word in context */
            int gen_len = out_pos - byte_start;
            if (gen_len > 0) {
                uint16_t wid = word_to_id(output + byte_start, gen_len);
                if (ctx_count < 64) ctx_ids[ctx_count++] = wid;
            }
            words_generated++;
        }

        /* Stop on sentence-ending punctuation */
        if (out_pos > 0) {
            char last = output[out_pos - 1];
            if (last == '.' || last == '!' || last == '?') break;
        }
    }

    output[out_pos] = '\0';
    return out_pos;
}

int chat_word_count(void) {
    return word_vocab_count;
}
