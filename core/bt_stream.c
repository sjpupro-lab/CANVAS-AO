/*
 * bt_stream.c — Phase D: BT Brain Stream 인코더/디코더
 *
 * 물의 흐름을 기록한다.
 * 128MB 캔버스 → 활성 셀만 → 수 MB 스트림 파일.
 */
#include "bt_stream.h"
#include "bt_canvas_render.h"
#include <string.h>
#include <stdlib.h>

/* ── CRC32 (ISO 3309, CVP와 동일) ── */
static uint32_t crc32_byte(uint32_t crc, uint8_t b) {
    crc ^= b;
    for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
    return crc;
}

static uint32_t crc32(const void *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++)
        crc = crc32_byte(crc, p[i]);
    return crc ^ 0xFFFFFFFFu;
}

/* ── 캔버스 해시 (FNV-1a, 활성 셀만) ── */
uint32_t btc_canvas_hash(const BtCanvas *c) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        if (c->cells[i].G == 0) continue;
        const BtcCell *cell = &c->cells[i];
        h ^= cell->A;          h *= 16777619u;
        h ^= (uint32_t)cell->B; h *= 16777619u;
        h ^= (uint32_t)cell->G; h *= 16777619u;
        h ^= (uint32_t)cell->R; h *= 16777619u;
    }
    return h;
}

/* ══════════════════════════════════════════════ */

int bt_stream_save(const BtCanvas *c, const char *path) {
    if (!c || !path) return BTS_ERR_IO;

    FILE *fp = fopen(path, "wb");
    if (!fp) return BTS_ERR_IO;

    /* ── 헤더 ── */
    BtStreamHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BTS_MAGIC;
    hdr.version = BTS_VERSION;
    hdr.frame_type = BTS_KEYFRAME;
    hdr.active_cells = c->used;
    hdr.total_trained = c->total_trained;
    hdr.canvas_hash = btc_canvas_hash(c);
    hdr.section_count = 2;  /* CELLS + META */
    hdr.header_crc = crc32(&hdr, 40);  /* CRC of first 40 bytes */

    fwrite(&hdr, sizeof(hdr), 1, fp);

    /* ── Section 1: NGRAM_CELLS ── */
    uint32_t cell_count = c->used;
    uint32_t cells_data_len = sizeof(uint32_t) + cell_count * sizeof(BtStreamCell);

    /* CRC를 위해 먼저 메모리에 구성 */
    uint8_t *cells_buf = (uint8_t *)malloc(cells_data_len);
    if (!cells_buf) { fclose(fp); return BTS_ERR_ALLOC; }

    memcpy(cells_buf, &cell_count, sizeof(uint32_t));
    uint32_t offset = sizeof(uint32_t);

    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        if (c->cells[i].G == 0) continue;
        BtStreamCell sc;
        sc.x = (uint16_t)(i % BTC_W);
        sc.y = (uint16_t)(i / BTC_W);
        sc.A = c->cells[i].A;
        sc.B = c->cells[i].B;
        sc.G = c->cells[i].G;
        sc.R = c->cells[i].R;
        sc.pad = c->cells[i].pad;
        memcpy(cells_buf + offset, &sc, sizeof(BtStreamCell));
        offset += sizeof(BtStreamCell);
    }

    BtStreamTlv tlv1;
    tlv1.type = BTS_SEC_CELLS;
    tlv1.len = cells_data_len;
    tlv1.crc = crc32(cells_buf, cells_data_len);
    fwrite(&tlv1, sizeof(tlv1), 1, fp);
    fwrite(cells_buf, 1, cells_data_len, fp);
    free(cells_buf);

    /* ── Section 2: METADATA ── */
    BtStreamMeta meta;
    memset(&meta, 0, sizeof(meta));
    meta.total_trained = c->total_trained;
    meta.total_predicted = c->total_predicted;
    meta.used = c->used;
    meta.collisions = c->collisions;
    memcpy(meta.window, c->window, 32);
    meta.win_len = (uint8_t)c->win_len;

    BtStreamTlv tlv2;
    tlv2.type = BTS_SEC_META;
    tlv2.len = sizeof(meta);
    tlv2.crc = crc32(&meta, sizeof(meta));
    fwrite(&tlv2, sizeof(tlv2), 1, fp);
    fwrite(&meta, sizeof(meta), 1, fp);

    fclose(fp);
    return BTS_OK;
}

/* ══════════════════════════════════════════════ */

int bt_stream_load(BtCanvas *c, const char *path) {
    if (!c || !path) return BTS_ERR_IO;

    FILE *fp = fopen(path, "rb");
    if (!fp) return BTS_ERR_IO;

    /* ── 헤더 검증 ── */
    BtStreamHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return BTS_ERR_IO; }
    if (hdr.magic != BTS_MAGIC) { fclose(fp); return BTS_ERR_MAGIC; }
    if (hdr.version != BTS_VERSION) { fclose(fp); return BTS_ERR_VERSION; }

    uint32_t check_crc = crc32(&hdr, 40);
    if (check_crc != hdr.header_crc) { fclose(fp); return BTS_ERR_CRC; }

    /* 캔버스 초기화 */
    memset(c->cells, 0, sizeof(c->cells));
    c->used = 0;

    /* ── 섹션 읽기 ── */
    for (uint32_t s = 0; s < hdr.section_count; s++) {
        BtStreamTlv tlv;
        if (fread(&tlv, sizeof(tlv), 1, fp) != 1) { fclose(fp); return BTS_ERR_IO; }

        uint8_t *data = (uint8_t *)malloc(tlv.len);
        if (!data) { fclose(fp); return BTS_ERR_ALLOC; }
        if (fread(data, 1, tlv.len, fp) != tlv.len) { free(data); fclose(fp); return BTS_ERR_IO; }

        /* CRC 검증 */
        if (crc32(data, tlv.len) != tlv.crc) { free(data); fclose(fp); return BTS_ERR_CRC; }

        if (tlv.type == BTS_SEC_CELLS) {
            uint32_t cell_count;
            memcpy(&cell_count, data, sizeof(uint32_t));
            uint32_t off = sizeof(uint32_t);

            for (uint32_t i = 0; i < cell_count; i++) {
                BtStreamCell sc;
                memcpy(&sc, data + off, sizeof(BtStreamCell));
                off += sizeof(BtStreamCell);

                uint32_t idx = (uint32_t)sc.y * BTC_W + (uint32_t)sc.x;
                if (idx < BTC_TOTAL) {
                    c->cells[idx].A = sc.A;
                    c->cells[idx].B = sc.B;
                    c->cells[idx].G = sc.G;
                    c->cells[idx].R = sc.R;
                    c->cells[idx].pad = sc.pad;
                    c->used++;
                }
            }
        } else if (tlv.type == BTS_SEC_META) {
            BtStreamMeta meta;
            memcpy(&meta, data, sizeof(meta));
            c->total_trained = meta.total_trained;
            c->total_predicted = meta.total_predicted;
            c->collisions = meta.collisions;
            memcpy(c->window, meta.window, 32);
            c->win_len = meta.win_len;
        }
        /* 미지 섹션은 건너뜀 (확장성) */

        free(data);
    }

    fclose(fp);

    /* 해시 검증 */
    uint32_t loaded_hash = btc_canvas_hash(c);
    if (loaded_hash != hdr.canvas_hash) return BTS_ERR_CRC;

    return BTS_OK;
}

/* ══════════════════════════════════════════════ */

int bt_stream_validate(const char *path) {
    if (!path) return BTS_ERR_IO;

    FILE *fp = fopen(path, "rb");
    if (!fp) return BTS_ERR_IO;

    BtStreamHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return BTS_ERR_IO; }
    if (hdr.magic != BTS_MAGIC) { fclose(fp); return BTS_ERR_MAGIC; }
    if (crc32(&hdr, 40) != hdr.header_crc) { fclose(fp); return BTS_ERR_CRC; }

    for (uint32_t s = 0; s < hdr.section_count; s++) {
        BtStreamTlv tlv;
        if (fread(&tlv, sizeof(tlv), 1, fp) != 1) { fclose(fp); return BTS_ERR_IO; }

        uint8_t *data = (uint8_t *)malloc(tlv.len);
        if (!data) { fclose(fp); return BTS_ERR_ALLOC; }
        if (fread(data, 1, tlv.len, fp) != tlv.len) { free(data); fclose(fp); return BTS_ERR_IO; }
        if (crc32(data, tlv.len) != tlv.crc) { free(data); fclose(fp); return BTS_ERR_CRC; }
        free(data);
    }

    fclose(fp);
    return BTS_OK;
}

/* ══════════════════════════════════════════════ */

int bt_stream_info(const char *path, BtStreamHeader *hdr) {
    if (!path || !hdr) return BTS_ERR_IO;

    FILE *fp = fopen(path, "rb");
    if (!fp) return BTS_ERR_IO;

    if (fread(hdr, sizeof(*hdr), 1, fp) != 1) { fclose(fp); return BTS_ERR_IO; }
    fclose(fp);

    if (hdr->magic != BTS_MAGIC) return BTS_ERR_MAGIC;
    if (crc32(hdr, 40) != hdr->header_crc) return BTS_ERR_CRC;
    return BTS_OK;
}

/* ══════════════════════════════════════════════
 * Level 2: 학습 애니메이션 프레임 내보내기
 * ══════════════════════════════════════════════ */

int bt_stream_export_frame(const BtCanvas *c, const char *dir, int frame_idx) {
    const char *out_dir = (dir && dir[0]) ? dir : "frames";
    return btcr_export_frame(c, out_dir, frame_idx);
}

int bt_stream_export_delta(const BtCanvas *before, const BtCanvas *after,
                            const char *dir, int frame_idx) {
    const char *out_dir = (dir && dir[0]) ? dir : "frames";
    return btcr_export_delta_frame(before, after, out_dir, frame_idx);
}
