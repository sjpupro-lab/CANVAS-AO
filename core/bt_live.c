/*
 * bt_live.c — Phase D2: 살아있는 AI 세션
 *
 * 살아있는 뇌를 직렬화. 로드하면 중단점에서 이어진다.
 */
#include "bt_live.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

/* CRC32 (bt_stream/bt_delta와 동일) */
static uint32_t crc32_b(uint32_t crc, uint8_t b) {
    crc ^= b;
    for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
    return crc;
}
static uint32_t crc32(const void *data, size_t len) {
    uint32_t c = 0xFFFFFFFFu;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) c = crc32_b(c, p[i]);
    return c ^ 0xFFFFFFFFu;
}

/* 간단한 랜덤 ID (진짜 UUID는 아니지만 충분히 고유) */
static uint32_t s_id_counter = 0;
static void gen_session_id(uint8_t id[16], const BtCanvas *c) {
    uint32_t seed = (uint32_t)time(NULL) ^ c->total_trained ^ c->used;
    seed ^= (uint32_t)(++s_id_counter) * 0x9e3779b9u;
    seed ^= (uint32_t)((uintptr_t)c & 0xFFFFFFFFu);
    for (int i = 0; i < 16; i++) {
        seed ^= seed >> 13;
        seed *= 0x5bd1e995u;
        seed ^= seed >> 15;
        seed += (uint32_t)i;
        id[i] = (uint8_t)(seed >> (i % 4 * 8));
    }
}

/* ══════════════════════════════════════════════ */

void bt_live_init(BtLiveSession *s, BtCanvas *c,
                  const char *delta_dir, uint8_t domain) {
    if (!s || !c) return;
    memset(s, 0, sizeof(*s));

    s->canvas = c;
    bt_engine_init(&s->engine, c);

    /* 헤더 초기화 */
    s->header.magic = BTL_MAGIC;
    s->header.version = BTL_VERSION;
    s->header.domain = domain;
    s->header.created_at = (uint64_t)time(NULL);
    gen_session_id(s->header.session_id, c);

    /* DELTA 디렉토리 */
    if (delta_dir) {
        strncpy(s->delta_dir, delta_dir, 255);
        s->delta_dir[255] = '\0';
        mkdir(s->delta_dir, 0755);
    }

    /* 스냅샷 할당 */
    s->prev_snapshot = (BtCanvas *)calloc(1, sizeof(BtCanvas));
    if (s->prev_snapshot) {
        memcpy(s->prev_snapshot, c, sizeof(BtCanvas));
        s->prev_hash = btc_canvas_hash(c);
    }
}

/* ══════════════════════════════════════════════ */

uint8_t bt_live_feed(BtLiveSession *s, uint8_t input_byte) {
    if (!s || !s->canvas) return 0;

    /* 1. 학습 */
    bt_engine_train(&s->engine, input_byte);

    /* 2. DK-EVAP: 512바이트마다 증발 */
    s->bytes_since_evap++;
    if (s->bytes_since_evap >= BTL_EVAP_INTERVAL) {
        btc_evaporate(s->canvas);
        s->bytes_since_evap = 0;
    }

    /* 3. GOP: 4096바이트마다 DELTA checkpoint */
    s->bytes_since_checkpoint++;
    if (s->bytes_since_checkpoint >= BTL_GOP_MAX_BYTES) {
        bt_live_checkpoint(s);
        s->bytes_since_checkpoint = 0;
    }

    /* 4. 예측 */
    BtDecision dec;
    s->last_predicted = bt_engine_decide(&s->engine, &dec);
    s->last_energy = dec.final_brightness;

    /* 5. tick 진행 */
    s->current_tick++;

    return s->last_predicted;
}

/* ══════════════════════════════════════════════ */

int bt_live_checkpoint(BtLiveSession *s) {
    if (!s || !s->canvas || !s->prev_snapshot) return BTL_ERR_IO;
    if (s->delta_dir[0] == '\0') return BTL_ERR_IO;

    char path[512];
    snprintf(path, sizeof(path), "%s/bt_brain_%04d.delta",
             s->delta_dir, s->delta_count);

    int rc = bt_delta_save(s->prev_snapshot, s->canvas, path,
                           s->delta_count, s->prev_hash,
                           s->current_tick > BTL_GOP_MAX_BYTES
                               ? s->current_tick - BTL_GOP_MAX_BYTES : 0,
                           s->current_tick);
    if (rc != BTD_OK) return rc;

    /* 새 스냅샷 */
    memcpy(s->prev_snapshot, s->canvas, sizeof(BtCanvas));
    s->prev_hash = btc_canvas_hash(s->canvas);
    s->delta_count++;
    s->header.delta_count = (uint32_t)s->delta_count;

    return BTL_OK;
}

/* ══════════════════════════════════════════════ */

int bt_live_save(const BtLiveSession *s, const char *path) {
    if (!s || !s->canvas || !path) return BTL_ERR_IO;

    /* 헤더 업데이트 */
    BtLiveHeader hdr = s->header;
    hdr.saved_at = (uint64_t)time(NULL);
    hdr.canvas_hash = btc_canvas_hash(s->canvas);
    hdr.total_trained = s->canvas->total_trained;
    hdr.delta_count = (uint32_t)s->delta_count;

    /* 컨텍스트 윈도우 복사 */
    hdr.context_len = (uint8_t)(s->engine.win_len > 32 ? 32 : s->engine.win_len);
    memcpy(hdr.context_window, s->engine.window, 32);

    /* CRC (header_crc 필드 직전까지) */
    hdr.header_crc = crc32(&hdr, sizeof(hdr) - 4);

    /* 파일 쓰기: [Header][KEYFRAME] */
    FILE *fp = fopen(path, "wb");
    if (!fp) return BTL_ERR_IO;
    fwrite(&hdr, sizeof(hdr), 1, fp);
    fclose(fp);

    /* KEYFRAME을 .stream 임시 파일로 저장 후 바이트 추가 */
    char stream_path[512];
    snprintf(stream_path, sizeof(stream_path), "%s._keyframe", path);
    int rc = bt_stream_save(s->canvas, stream_path);
    if (rc != BTS_OK) return BTL_ERR_IO;

    /* .stream 바이트를 .btlive에 append */
    FILE *src = fopen(stream_path, "rb");
    fp = fopen(path, "ab");
    if (!src || !fp) {
        if (src) fclose(src);
        if (fp) fclose(fp);
        remove(stream_path);
        return BTL_ERR_IO;
    }

    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
        fwrite(buf, 1, n, fp);

    fclose(src);
    fclose(fp);
    remove(stream_path);

    return BTL_OK;
}

/* ══════════════════════════════════════════════ */

int bt_live_load(BtLiveSession *s, const char *path) {
    if (!s || !s->canvas || !path) return BTL_ERR_IO;

    FILE *fp = fopen(path, "rb");
    if (!fp) return BTL_ERR_IO;

    /* 헤더 읽기 */
    BtLiveHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return BTL_ERR_IO; }
    if (hdr.magic != BTL_MAGIC) { fclose(fp); return BTL_ERR_MAGIC; }

    uint32_t check = crc32(&hdr, sizeof(hdr) - 4);
    if (check != hdr.header_crc) { fclose(fp); return BTL_ERR_CRC; }

    /* KEYFRAME 바이트를 임시 파일로 추출 */
    char stream_path[512];
    snprintf(stream_path, sizeof(stream_path), "%s._load_key", path);
    FILE *tmp = fopen(stream_path, "wb");
    if (!tmp) { fclose(fp); return BTL_ERR_IO; }

    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        fwrite(buf, 1, n, tmp);

    fclose(fp);
    fclose(tmp);

    /* KEYFRAME 로드 */
    int rc = bt_stream_load(s->canvas, stream_path);
    remove(stream_path);
    if (rc != BTS_OK) return BTL_ERR_IO;

    /* 헤더 복원 */
    s->header = hdr;

    /* 엔진 재초기화 + 컨텍스트 윈도우 복원 */
    bt_engine_init(&s->engine, s->canvas);
    s->engine.win_len = hdr.context_len;
    memcpy(s->engine.window, hdr.context_window, 32);

    /* 런타임 상태 복원 */
    s->delta_count = (int)hdr.delta_count;
    s->current_tick = hdr.saved_at;
    s->bytes_since_evap = 0;
    s->bytes_since_checkpoint = 0;

    /* 스냅샷 갱신 */
    if (!s->prev_snapshot)
        s->prev_snapshot = (BtCanvas *)calloc(1, sizeof(BtCanvas));
    if (s->prev_snapshot) {
        memcpy(s->prev_snapshot, s->canvas, sizeof(BtCanvas));
        s->prev_hash = btc_canvas_hash(s->canvas);
    }

    return BTL_OK;
}

/* ══════════════════════════════════════════════ */

void bt_live_free(BtLiveSession *s) {
    if (!s) return;
    free(s->prev_snapshot);
    s->prev_snapshot = NULL;
}

/* ══ Phase D: Predicted Checkpoint ══════════════ */

int bt_live_checkpoint_pred(BtLiveSession *s, uint32_t evap_rounds) {
    if (!s || !s->canvas || !s->prev_snapshot) return BTL_ERR_IO;
    if (s->delta_dir[0] == '\0') return BTL_ERR_IO;

    char path[512];
    snprintf(path, sizeof(path), "%s/bt_brain_%04d.delta",
             s->delta_dir, s->delta_count);

    int rc = bt_delta_save_pred(s->prev_snapshot, s->canvas, path,
                                s->delta_count, s->prev_hash,
                                s->current_tick > BTL_GOP_MAX_BYTES
                                    ? s->current_tick - BTL_GOP_MAX_BYTES : 0,
                                s->current_tick,
                                evap_rounds);
    if (rc != BTD_OK) return rc;

    /* 새 스냅샷 */
    memcpy(s->prev_snapshot, s->canvas, sizeof(BtCanvas));
    s->prev_hash = btc_canvas_hash(s->canvas);
    s->delta_count++;
    s->header.delta_count = (uint32_t)s->delta_count;

    return BTL_OK;
}

uint8_t bt_live_feed_pred(BtLiveSession *s, uint8_t input_byte,
                          uint32_t evap_rounds) {
    if (!s || !s->canvas) return 0;

    /* 1. 학습 */
    bt_engine_train(&s->engine, input_byte);

    /* 2. DK-EVAP: 512바이트마다 증발 */
    s->bytes_since_evap++;
    if (s->bytes_since_evap >= BTL_EVAP_INTERVAL) {
        btc_evaporate(s->canvas);
        s->bytes_since_evap = 0;
    }

    /* 3. GOP: 4096바이트마다 Predicted DELTA checkpoint */
    s->bytes_since_checkpoint++;
    if (s->bytes_since_checkpoint >= BTL_GOP_MAX_BYTES) {
        bt_live_checkpoint_pred(s, evap_rounds);
        s->bytes_since_checkpoint = 0;
    }

    /* 4. 예측 */
    BtDecision dec;
    s->last_predicted = bt_engine_decide(&s->engine, &dec);
    s->last_energy = dec.final_brightness;

    /* 5. tick 진행 */
    s->current_tick++;

    return s->last_predicted;
}

/* ══ SJQ-B: 데이터 압축 (예측 잔차 인코딩) ══════════ */

int bt_sjqb_encode(BtLiveSession *s,
                   const uint8_t *data, size_t len,
                   const char *path,
                   SjqbStats *stats)
{
    if (!s || !data || !path) return SJQB_ERR_IO;

    /* 교정값 배열 */
    uint32_t corr_cap = 4096;
    SjqbCorrection *corrs = malloc(corr_cap * sizeof(SjqbCorrection));
    if (!corrs) return SJQB_ERR_ALLOC;

    uint32_t corr_count = 0;
    uint32_t evap_counter = 0;

    for (size_t i = 0; i < len; i++) {
        /* 1. 예측 */
        BtDecision dec;
        uint8_t predicted = bt_engine_decide(&s->engine, &dec);

        /* 2. XOR 교정값 */
        uint8_t correction = data[i] ^ predicted;
        if (correction != 0) {
            if (corr_count >= corr_cap) {
                corr_cap *= 2;
                corrs = realloc(corrs, corr_cap * sizeof(SjqbCorrection));
                if (!corrs) return SJQB_ERR_ALLOC;
            }
            corrs[corr_count].offset = (uint32_t)i;
            corrs[corr_count].correction = correction;
            corr_count++;
        }

        /* 3. 학습 (실제 바이트로) */
        bt_engine_train(&s->engine, data[i]);

        /* 4. DK-EVAP */
        evap_counter++;
        if (evap_counter >= BTL_EVAP_INTERVAL) {
            btc_evaporate(s->canvas);
            evap_counter = 0;
        }
    }

    /* 헤더 작성 */
    SjqbHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = SJQB_MAGIC;
    hdr.version = SJQB_VERSION;
    hdr.original_len = (uint32_t)len;
    hdr.correction_count = corr_count;
    hdr.canvas_hash = btc_canvas_hash(s->canvas);
    hdr.header_crc = crc32(&hdr, 20);

    /* 파일 쓰기 */
    FILE *fp = fopen(path, "wb");
    if (!fp) { free(corrs); return SJQB_ERR_IO; }

    fwrite(&hdr, sizeof(hdr), 1, fp);
    if (corr_count > 0)
        fwrite(corrs, sizeof(SjqbCorrection), corr_count, fp);

    fclose(fp);

    /* 통계 */
    if (stats) {
        stats->original_len = (uint32_t)len;
        stats->correction_count = corr_count;
        stats->file_size = (uint32_t)(sizeof(SjqbHeader) +
                           corr_count * sizeof(SjqbCorrection));
        stats->hit_rate_x1000 = len > 0
            ? (uint32_t)((uint64_t)(len - corr_count) * 1000 / len)
            : 0;
        stats->comp_ratio_x100 = stats->file_size > 0
            ? (uint32_t)((uint64_t)len * 100 / stats->file_size)
            : 0;
    }

    free(corrs);
    return SJQB_OK;
}

int bt_sjqb_decode(BtLiveSession *s,
                   const char *path,
                   uint8_t *out, size_t *out_len)
{
    if (!s || !path || !out || !out_len) return SJQB_ERR_IO;

    FILE *fp = fopen(path, "rb");
    if (!fp) return SJQB_ERR_IO;

    /* 헤더 읽기 */
    SjqbHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return SJQB_ERR_IO; }
    if (hdr.magic != SJQB_MAGIC) { fclose(fp); return SJQB_ERR_MAGIC; }
    if (crc32(&hdr, 20) != hdr.header_crc) { fclose(fp); return SJQB_ERR_CRC; }

    /* 교정값 읽기 */
    SjqbCorrection *corrs = NULL;
    if (hdr.correction_count > 0) {
        corrs = malloc(hdr.correction_count * sizeof(SjqbCorrection));
        if (!corrs) { fclose(fp); return SJQB_ERR_ALLOC; }
        if (fread(corrs, sizeof(SjqbCorrection), hdr.correction_count, fp)
            != hdr.correction_count) {
            free(corrs); fclose(fp); return SJQB_ERR_IO;
        }
    }
    fclose(fp);

    /* 디코딩 */
    uint32_t corr_idx = 0;
    uint32_t evap_counter = 0;

    for (uint32_t i = 0; i < hdr.original_len; i++) {
        /* 1. 예측 */
        BtDecision dec;
        uint8_t predicted = bt_engine_decide(&s->engine, &dec);

        /* 2. 교정값 적용 */
        uint8_t byte;
        if (corr_idx < hdr.correction_count &&
            corrs[corr_idx].offset == i) {
            byte = predicted ^ corrs[corr_idx].correction;
            corr_idx++;
        } else {
            byte = predicted;
        }

        out[i] = byte;

        /* 3. 학습 (인코더와 동기화) */
        bt_engine_train(&s->engine, byte);

        /* 4. DK-EVAP */
        evap_counter++;
        if (evap_counter >= BTL_EVAP_INTERVAL) {
            btc_evaporate(s->canvas);
            evap_counter = 0;
        }
    }

    *out_len = hdr.original_len;
    free(corrs);
    return SJQB_OK;
}

/* ══ SJQ-B v2: RLE 비트맵 교정값 ══════════════════ */

int bt_sjqb_encode_v2(BtLiveSession *s,
                      const uint8_t *data, size_t len,
                      const char *path,
                      SjqbStats *stats)
{
    if (!s || !data || !path) return SJQB_ERR_IO;

    /* 1단계: 적중/미스 비트맵 + 교정값 수집 */
    uint8_t *bitmap = calloc(len, 1);   /* 0=hit, 1=miss */
    uint8_t *corrections = malloc(len);
    if (!bitmap || !corrections) {
        free(bitmap); free(corrections);
        return SJQB_ERR_ALLOC;
    }

    uint32_t miss_count = 0;
    uint32_t evap_counter = 0;

    for (size_t i = 0; i < len; i++) {
        BtDecision dec;
        uint8_t predicted = bt_engine_decide(&s->engine, &dec);
        uint8_t correction = data[i] ^ predicted;

        if (correction != 0) {
            bitmap[i] = 1;
            corrections[miss_count++] = correction;
        }

        bt_engine_train(&s->engine, data[i]);

        evap_counter++;
        if (evap_counter >= BTL_EVAP_INTERVAL) {
            btc_evaporate(s->canvas);
            evap_counter = 0;
        }
    }

    /* 2단계: 비트맵 → RLE 압축 */
    /* 0x00~0x7F: N+1 hits, 0x80~0xFF: (N-0x80)+1 misses */
    uint8_t *rle = malloc(len + 256);
    if (!rle) { free(bitmap); free(corrections); return SJQB_ERR_ALLOC; }

    uint32_t rle_len = 0;
    size_t pos = 0;
    while (pos < len) {
        uint8_t val = bitmap[pos];
        size_t run = 0;
        while (pos + run < len && bitmap[pos + run] == val && run < 128) {
            run++;
        }
        if (val == 0) {
            /* hit run */
            rle[rle_len++] = (uint8_t)(run - 1);          /* 0x00~0x7F */
        } else {
            /* miss run */
            rle[rle_len++] = (uint8_t)(0x80 + run - 1);   /* 0x80~0xFF */
        }
        pos += run;
    }

    /* 3단계: 헤더 + 파일 쓰기 */
    SjqbV2Header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = SJQB_V2_MAGIC;
    hdr.version = 0x00020000u;
    hdr.original_len = (uint32_t)len;
    hdr.correction_count = miss_count;
    hdr.bitmap_rle_len = rle_len;
    hdr.canvas_hash = btc_canvas_hash(s->canvas);
    hdr.header_crc = crc32(&hdr, sizeof(hdr) - 4);

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(bitmap); free(corrections); free(rle); return SJQB_ERR_IO; }

    fwrite(&hdr, sizeof(hdr), 1, fp);
    fwrite(rle, 1, rle_len, fp);
    fwrite(corrections, 1, miss_count, fp);

    fclose(fp);

    /* 통계 */
    if (stats) {
        stats->original_len = (uint32_t)len;
        stats->correction_count = miss_count;
        stats->file_size = (uint32_t)(sizeof(SjqbV2Header) + rle_len + miss_count);
        stats->hit_rate_x1000 = len > 0
            ? (uint32_t)((uint64_t)(len - miss_count) * 1000 / len) : 0;
        stats->comp_ratio_x100 = stats->file_size > 0
            ? (uint32_t)((uint64_t)len * 100 / stats->file_size) : 0;
    }

    free(bitmap); free(corrections); free(rle);
    return SJQB_OK;
}

int bt_sjqb_decode_v2(BtLiveSession *s,
                      const char *path,
                      uint8_t *out, size_t *out_len)
{
    if (!s || !path || !out || !out_len) return SJQB_ERR_IO;

    FILE *fp = fopen(path, "rb");
    if (!fp) return SJQB_ERR_IO;

    SjqbV2Header hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return SJQB_ERR_IO; }
    if (hdr.magic != SJQB_V2_MAGIC) { fclose(fp); return SJQB_ERR_MAGIC; }
    if (crc32(&hdr, sizeof(hdr) - 4) != hdr.header_crc) {
        fclose(fp); return SJQB_ERR_CRC;
    }

    /* RLE 비트맵 읽기 */
    uint8_t *rle = malloc(hdr.bitmap_rle_len);
    if (!rle) { fclose(fp); return SJQB_ERR_ALLOC; }
    if (fread(rle, 1, hdr.bitmap_rle_len, fp) != hdr.bitmap_rle_len) {
        free(rle); fclose(fp); return SJQB_ERR_IO;
    }

    /* 교정값 읽기 */
    uint8_t *corrections = malloc(hdr.correction_count);
    if (!corrections && hdr.correction_count > 0) {
        free(rle); fclose(fp); return SJQB_ERR_ALLOC;
    }
    if (hdr.correction_count > 0) {
        if (fread(corrections, 1, hdr.correction_count, fp)
            != hdr.correction_count) {
            free(rle); free(corrections); fclose(fp); return SJQB_ERR_IO;
        }
    }
    fclose(fp);

    /* RLE → 비트맵 복원 */
    uint8_t *bitmap = calloc(hdr.original_len, 1);
    if (!bitmap) { free(rle); free(corrections); return SJQB_ERR_ALLOC; }

    uint32_t bpos = 0;
    for (uint32_t ri = 0; ri < hdr.bitmap_rle_len && bpos < hdr.original_len; ri++) {
        uint8_t code = rle[ri];
        if (code <= 0x7F) {
            /* hit run: N+1 zeros */
            uint32_t run = (uint32_t)(code & 0x7F) + 1;
            for (uint32_t r = 0; r < run && bpos < hdr.original_len; r++)
                bitmap[bpos++] = 0;
        } else {
            /* miss run: (N-0x80)+1 ones */
            uint32_t run = (uint32_t)(code - 0x80) + 1;
            for (uint32_t r = 0; r < run && bpos < hdr.original_len; r++)
                bitmap[bpos++] = 1;
        }
    }

    /* 디코딩 */
    uint32_t corr_idx = 0;
    uint32_t evap_counter = 0;

    for (uint32_t i = 0; i < hdr.original_len; i++) {
        BtDecision dec;
        uint8_t predicted = bt_engine_decide(&s->engine, &dec);

        uint8_t byte;
        if (bitmap[i]) {
            byte = predicted ^ corrections[corr_idx++];
        } else {
            byte = predicted;
        }

        out[i] = byte;
        bt_engine_train(&s->engine, byte);

        evap_counter++;
        if (evap_counter >= BTL_EVAP_INTERVAL) {
            btc_evaporate(s->canvas);
            evap_counter = 0;
        }
    }

    *out_len = hdr.original_len;

    free(bitmap); free(rle); free(corrections);
    return SJQB_OK;
}

/* ══ v6f 인코딩 ════════════════════════════════════ */

int v6f_encode_csv_prices(const int64_t *prices_x100, int count,
                          uint8_t *out) {
    if (!prices_x100 || !out || count < 2) return 0;

    for (int i = 1; i < count; i++) {
        int64_t prev = prices_x100[i - 1];
        if (prev == 0) prev = 1;
        /* 변화율 × 100 (정수) */
        int32_t pct_x100 = (int32_t)((prices_x100[i] - prev) * 10000 / prev);
        out[i - 1] = v6f_encode_price_change(pct_x100);
    }
    return count - 1;
}

/* ══ 교정값 클러스터링 (이상 감지) ══════════════════ */

int sjqb_find_anomalies(const char *sjqb_v2_path,
                        int min_run,
                        SjqbAnomaly *out, int max_out) {
    if (!sjqb_v2_path || !out || max_out <= 0) return 0;
    if (min_run < 2) min_run = 2;

    FILE *fp = fopen(sjqb_v2_path, "rb");
    if (!fp) return 0;

    SjqbV2Header hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return 0; }
    if (hdr.magic != SJQB_V2_MAGIC) { fclose(fp); return 0; }

    /* RLE 비트맵 읽기 */
    uint8_t *rle = malloc(hdr.bitmap_rle_len);
    if (!rle) { fclose(fp); return 0; }
    if (fread(rle, 1, hdr.bitmap_rle_len, fp) != hdr.bitmap_rle_len) {
        free(rle); fclose(fp); return 0;
    }

    /* 교정값 읽기 */
    uint8_t *corrections = malloc(hdr.correction_count + 1);
    if (!corrections) { free(rle); fclose(fp); return 0; }
    if (hdr.correction_count > 0) {
        if (fread(corrections, 1, hdr.correction_count, fp)
            != hdr.correction_count) {
            free(rle); free(corrections); fclose(fp); return 0;
        }
    }
    fclose(fp);

    /* RLE → 비트맵 복원 + 이상 구간 검출 */
    int anomaly_count = 0;
    uint32_t pos = 0;
    uint32_t corr_idx = 0;

    for (uint32_t ri = 0; ri < hdr.bitmap_rle_len && pos < hdr.original_len; ri++) {
        uint8_t code = rle[ri];

        if (code <= 0x7F) {
            /* hit run */
            uint32_t run = (uint32_t)(code & 0x7F) + 1;
            pos += run;
        } else {
            /* miss run */
            uint32_t run = (uint32_t)(code - 0x80) + 1;

            if ((int)run >= min_run && anomaly_count < max_out) {
                /* 이상 구간 발견 */
                SjqbAnomaly *a = &out[anomaly_count];
                a->start = pos;
                a->length = run;
                a->intensity = 0;

                /* 교정값 강도 합산 */
                for (uint32_t j = 0; j < run && corr_idx + j < hdr.correction_count; j++) {
                    a->intensity += corrections[corr_idx + j];
                }
                anomaly_count++;
            }

            corr_idx += run;
            pos += run;
        }
    }

    free(rle);
    free(corrections);
    return anomaly_count;
}

int bt_sjqb_info(const char *path, SjqbHeader *out_hdr) {
    if (!path || !out_hdr) return SJQB_ERR_IO;
    FILE *fp = fopen(path, "rb");
    if (!fp) return SJQB_ERR_IO;
    if (fread(out_hdr, sizeof(*out_hdr), 1, fp) != 1) {
        fclose(fp); return SJQB_ERR_IO;
    }
    fclose(fp);
    if (out_hdr->magic != SJQB_MAGIC) return SJQB_ERR_MAGIC;
    if (crc32(out_hdr, 20) != out_hdr->header_crc) return SJQB_ERR_CRC;
    return SJQB_OK;
}
