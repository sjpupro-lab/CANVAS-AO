/*
 * bt_delta.c — Phase D1: DELTA 프레임 인코더/디코더
 *
 * KEYFRAME 이후 변화된 셀만 저장.
 * SEC_ADDED: 새로 활성화된 셀 (before.G==0, after.G>0)
 * SEC_MODIFIED: G/R/pad 값이 변경된 셀
 * SEC_REMOVED: 소멸된 셀 (before.G>0, after.G==0)
 */
#include "bt_delta.h"
#include "bt_stream.h"  /* btc_canvas_hash 사용 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── CRC32 (bt_stream.c와 동일) ── */
static uint32_t crc32_b(uint32_t crc, uint8_t b) {
    crc ^= b;
    for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
    return crc;
}
static uint32_t crc32(const void *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) crc = crc32_b(crc, p[i]);
    return crc ^ 0xFFFFFFFFu;
}

/* ── TLV 쓰기/읽기 헬퍼 ── */
static int write_tlv(FILE *fp, uint8_t type, const void *data, uint32_t len) {
    BtDeltaTlv tlv = { .type = type, .len = len, .pad = 0 };
    if (fwrite(&tlv, sizeof(tlv), 1, fp) != 1) return -1;
    if (len > 0 && fwrite(data, 1, len, fp) != len) return -1;
    return 0;
}

/* ══════════════════════════════════════════════ */

int bt_delta_save(const BtCanvas *before,
                  const BtCanvas *after,
                  const char     *path,
                  int             frame_index,
                  uint32_t        ref_hash,
                  uint64_t        tick_start,
                  uint64_t        tick_end)
{
    if (!before || !after || !path) return BTD_ERR_IO;

    /* 1단계: 변화 셀 수집 */
    uint32_t added_cap = 4096, mod_cap = 4096, rem_cap = 4096;
    BtDeltaCell *added = malloc(added_cap * sizeof(BtDeltaCell));
    BtDeltaCell *modified = malloc(mod_cap * sizeof(BtDeltaCell));
    BtDeltaRemoved *removed = malloc(rem_cap * sizeof(BtDeltaRemoved));
    if (!added || !modified || !removed) {
        free(added); free(modified); free(removed);
        return BTD_ERR_ALLOC;
    }

    uint32_t n_added = 0, n_mod = 0, n_rem = 0;

    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        const BtcCell *b = &before->cells[i];
        const BtcCell *a = &after->cells[i];
        uint16_t x = (uint16_t)(i % BTC_W);
        uint16_t y = (uint16_t)(i / BTC_W);

        if (b->G == 0 && a->G > 0) {
            /* 신규 셀 */
            if (n_added >= added_cap) {
                added_cap *= 2;
                added = realloc(added, added_cap * sizeof(BtDeltaCell));
            }
            added[n_added++] = (BtDeltaCell){x, y, a->A, a->B, a->G, a->R, a->pad};
        } else if (b->G > 0 && a->G == 0) {
            /* 소멸 셀 */
            if (n_rem >= rem_cap) {
                rem_cap *= 2;
                removed = realloc(removed, rem_cap * sizeof(BtDeltaRemoved));
            }
            removed[n_rem++] = (BtDeltaRemoved){x, y};
        } else if (b->G > 0 && a->G > 0) {
            /* 변경 확인 */
            if (b->A != a->A || b->B != a->B || b->G != a->G ||
                b->R != a->R || b->pad != a->pad) {
                if (n_mod >= mod_cap) {
                    mod_cap *= 2;
                    modified = realloc(modified, mod_cap * sizeof(BtDeltaCell));
                }
                modified[n_mod++] = (BtDeltaCell){x, y, a->A, a->B, a->G, a->R, a->pad};
            }
        }
    }

    /* 2단계: 헤더 작성 */
    BtDeltaHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BTD_MAGIC;
    hdr.version = BTD_VERSION;
    hdr.frame_type = 2; /* DELTA */
    hdr.frame_index = (uint32_t)frame_index;
    hdr.ref_frame_hash = ref_hash;
    hdr.delta_cells = n_added + n_mod + n_rem;
    hdr.tick_start = tick_start;
    hdr.tick_end = tick_end;
    hdr.header_crc = crc32(&hdr, 36);

    /* 3단계: 파일 쓰기 */
    FILE *fp = fopen(path, "wb");
    if (!fp) { free(added); free(modified); free(removed); return BTD_ERR_IO; }

    fwrite(&hdr, sizeof(hdr), 1, fp);

    if (n_added > 0)
        write_tlv(fp, BTD_SEC_ADDED, added, n_added * sizeof(BtDeltaCell));
    if (n_mod > 0)
        write_tlv(fp, BTD_SEC_MODIFIED, modified, n_mod * sizeof(BtDeltaCell));
    if (n_rem > 0)
        write_tlv(fp, BTD_SEC_REMOVED, removed, n_rem * sizeof(BtDeltaRemoved));

    fclose(fp);
    free(added); free(modified); free(removed);
    return BTD_OK;
}

/* ══════════════════════════════════════════════ */

int bt_delta_apply(BtCanvas *canvas, const char *path) {
    if (!canvas || !path) return BTD_ERR_IO;

    FILE *fp = fopen(path, "rb");
    if (!fp) return BTD_ERR_IO;

    /* 헤더 읽기 + 검증 */
    BtDeltaHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return BTD_ERR_IO; }
    if (hdr.magic != BTD_MAGIC) { fclose(fp); return BTD_ERR_IO; }
    if (crc32(&hdr, 36) != hdr.header_crc) { fclose(fp); return BTD_ERR_CRC; }

    /* DK-CHAIN: ref_hash 검증 */
    uint32_t current_hash = btc_canvas_hash(canvas);
    if (current_hash != hdr.ref_frame_hash) {
        fclose(fp);
        return BTD_ERR_REF_HASH;
    }

    /* 섹션 읽기 + 적용 */
    while (!feof(fp)) {
        BtDeltaTlv tlv;
        if (fread(&tlv, sizeof(tlv), 1, fp) != 1) break;

        if (tlv.type == BTD_SEC_ADDED || tlv.type == BTD_SEC_MODIFIED) {
            uint32_t count = tlv.len / sizeof(BtDeltaCell);
            for (uint32_t i = 0; i < count; i++) {
                BtDeltaCell dc;
                if (fread(&dc, sizeof(dc), 1, fp) != 1) break;
                uint32_t idx = (uint32_t)dc.y * BTC_W + (uint32_t)dc.x;
                if (idx >= BTC_TOTAL) continue;
                int was_empty = (canvas->cells[idx].G == 0);
                canvas->cells[idx].A = dc.A;
                canvas->cells[idx].B = dc.B;
                canvas->cells[idx].G = dc.G;
                canvas->cells[idx].R = dc.R;
                canvas->cells[idx].pad = dc.pad;
                if (was_empty && dc.G > 0) canvas->used++;
            }
        } else if (tlv.type == BTD_SEC_REMOVED) {
            uint32_t count = tlv.len / sizeof(BtDeltaRemoved);
            for (uint32_t i = 0; i < count; i++) {
                BtDeltaRemoved dr;
                if (fread(&dr, sizeof(dr), 1, fp) != 1) break;
                uint32_t idx = (uint32_t)dr.y * BTC_W + (uint32_t)dr.x;
                if (idx >= BTC_TOTAL) continue;
                if (canvas->cells[idx].G > 0) canvas->used--;
                memset(&canvas->cells[idx], 0, sizeof(BtcCell));
            }
        } else {
            /* 미지 섹션 건너뜀 */
            fseek(fp, tlv.len, SEEK_CUR);
        }
    }

    fclose(fp);
    return BTD_OK;
}

/* ══════════════════════════════════════════════ */

int bt_delta_chain_apply(BtCanvas *canvas, const char *dir, int count) {
    if (!canvas || !dir) return BTD_ERR_IO;

    for (int i = 0; i < count; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/bt_brain_%04d.delta", dir, i);
        int rc = bt_delta_apply(canvas, path);
        if (rc != BTD_OK) return rc;
    }
    return BTD_OK;
}

/* ══════════════════════════════════════════════ */

int bt_delta_chain_seek(BtCanvas *canvas, const char *dir, uint64_t target_tick) {
    if (!canvas || !dir) return BTD_ERR_IO;

    for (int i = 0; i < 10000; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/bt_brain_%04d.delta", dir, i);

        BtDeltaHeader hdr;
        int rc = bt_delta_info(path, &hdr);
        if (rc != BTD_OK) break;  /* 파일 없음 = 체인 끝 */

        if (hdr.tick_end > target_tick) break;  /* 목표 tick 도달 */

        rc = bt_delta_apply(canvas, path);
        if (rc != BTD_OK) return rc;
    }
    return BTD_OK;
}

/* ══════════════════════════════════════════════ */

int bt_delta_info(const char *path, BtDeltaHeader *out_hdr) {
    if (!path || !out_hdr) return BTD_ERR_IO;

    FILE *fp = fopen(path, "rb");
    if (!fp) return BTD_ERR_IO;

    if (fread(out_hdr, sizeof(*out_hdr), 1, fp) != 1) {
        fclose(fp); return BTD_ERR_IO;
    }
    fclose(fp);

    if (out_hdr->magic != BTD_MAGIC) return BTD_ERR_IO;
    if (crc32(out_hdr, 36) != out_hdr->header_crc) return BTD_ERR_CRC;
    return BTD_OK;
}

/* ══ Phase D: Predicted Delta ══════════════════════ */
/*
 * 핵심: diff(simulate_evap(before, N), after) = 학습 잔차만
 *
 * 헤더 인코딩:
 *   frame_type = BTD_FRAME_PRED (3)
 *   tick_start의 상위 32비트에 evap_rounds를 팩킹하지 않음.
 *   대신 ref_frame_hash 다음 4바이트(delta_cells 위치 전)에
 *   evap_rounds를 별도 저장하면 헤더 포맷이 깨짐.
 *
 * 해결: 기존 40B 헤더의 frame_type=3으로 구분.
 *       evap_rounds는 TLV 섹션 0x04로 기록 (4바이트).
 *       → 기존 디코더는 미지 섹션으로 건너뜀 (안전).
 */

#define BTD_SEC_EVAP_ROUNDS 0x04u  /* Predicted delta: 증발 라운드 수 */

int bt_delta_save_pred(const BtCanvas *before,
                       const BtCanvas *after,
                       const char     *path,
                       int             frame_index,
                       uint32_t        ref_hash,
                       uint64_t        tick_start,
                       uint64_t        tick_end,
                       uint32_t        evap_rounds)
{
    if (!before || !after || !path) return BTD_ERR_IO;

    /* evap_rounds == 0이면 raw delta로 폴백 */
    if (evap_rounds == 0) {
        return bt_delta_save(before, after, path, frame_index,
                             ref_hash, tick_start, tick_end);
    }

    /* 1단계: 예측 상태 생성 (before에 증발 시뮬레이션) */
    BtCanvas *predicted = (BtCanvas *)calloc(1, sizeof(BtCanvas));
    if (!predicted) return BTD_ERR_ALLOC;

    btc_simulate_evaporate(before, predicted, evap_rounds);

    /* 2단계: diff(predicted, after) = 잔차 수집 */
    uint32_t added_cap = 4096, mod_cap = 4096, rem_cap = 4096;
    BtDeltaCell *added = malloc(added_cap * sizeof(BtDeltaCell));
    BtDeltaCell *modified = malloc(mod_cap * sizeof(BtDeltaCell));
    BtDeltaRemoved *removed = malloc(rem_cap * sizeof(BtDeltaRemoved));
    if (!added || !modified || !removed) {
        free(added); free(modified); free(removed); free(predicted);
        return BTD_ERR_ALLOC;
    }

    uint32_t n_added = 0, n_mod = 0, n_rem = 0;

    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        const BtcCell *p = &predicted->cells[i];  /* 예측 */
        const BtcCell *a = &after->cells[i];       /* 실제 */
        uint16_t x = (uint16_t)(i % BTC_W);
        uint16_t y = (uint16_t)(i / BTC_W);

        if (p->G == 0 && a->G > 0) {
            /* 예측에 없지만 실제에 있음 → ADDED */
            if (n_added >= added_cap) {
                added_cap *= 2;
                added = realloc(added, added_cap * sizeof(BtDeltaCell));
            }
            added[n_added++] = (BtDeltaCell){x, y, a->A, a->B, a->G, a->R, a->pad};
        } else if (p->G > 0 && a->G == 0) {
            /* 예측에 있지만 실제에 없음 → REMOVED */
            if (n_rem >= rem_cap) {
                rem_cap *= 2;
                removed = realloc(removed, rem_cap * sizeof(BtDeltaRemoved));
            }
            removed[n_rem++] = (BtDeltaRemoved){x, y};
        } else if (p->G > 0 && a->G > 0) {
            /* 양쪽 다 활성 → 값이 다르면 MODIFIED */
            if (p->A != a->A || p->B != a->B || p->G != a->G ||
                p->R != a->R || p->pad != a->pad) {
                if (n_mod >= mod_cap) {
                    mod_cap *= 2;
                    modified = realloc(modified, mod_cap * sizeof(BtDeltaCell));
                }
                modified[n_mod++] = (BtDeltaCell){x, y, a->A, a->B, a->G, a->R, a->pad};
            }
        }
    }

    free(predicted);

    /* 3단계: 헤더 (frame_type = PRED) */
    BtDeltaHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BTD_MAGIC;
    hdr.version = BTD_VERSION;
    hdr.frame_type = BTD_FRAME_PRED;  /* 3 = Predicted */
    hdr.frame_index = (uint32_t)frame_index;
    hdr.ref_frame_hash = ref_hash;
    hdr.delta_cells = n_added + n_mod + n_rem;
    hdr.tick_start = tick_start;
    hdr.tick_end = tick_end;
    hdr.header_crc = crc32(&hdr, 36);

    /* 4단계: 파일 쓰기 */
    FILE *fp = fopen(path, "wb");
    if (!fp) { free(added); free(modified); free(removed); return BTD_ERR_IO; }

    fwrite(&hdr, sizeof(hdr), 1, fp);

    /* evap_rounds를 TLV 섹션으로 기록 */
    write_tlv(fp, BTD_SEC_EVAP_ROUNDS, &evap_rounds, sizeof(evap_rounds));

    if (n_added > 0)
        write_tlv(fp, BTD_SEC_ADDED, added, n_added * sizeof(BtDeltaCell));
    if (n_mod > 0)
        write_tlv(fp, BTD_SEC_MODIFIED, modified, n_mod * sizeof(BtDeltaCell));
    if (n_rem > 0)
        write_tlv(fp, BTD_SEC_REMOVED, removed, n_rem * sizeof(BtDeltaRemoved));

    fclose(fp);
    free(added); free(modified); free(removed);
    return BTD_OK;
}

/* ══════════════════════════════════════════════ */

int bt_delta_apply_pred(BtCanvas *canvas, const char *path) {
    if (!canvas || !path) return BTD_ERR_IO;

    FILE *fp = fopen(path, "rb");
    if (!fp) return BTD_ERR_IO;

    /* 헤더 읽기 + 검증 */
    BtDeltaHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return BTD_ERR_IO; }
    if (hdr.magic != BTD_MAGIC) { fclose(fp); return BTD_ERR_IO; }
    if (crc32(&hdr, 36) != hdr.header_crc) { fclose(fp); return BTD_ERR_CRC; }

    /* DK-CHAIN: ref_hash 검증 */
    uint32_t current_hash = btc_canvas_hash(canvas);
    if (current_hash != hdr.ref_frame_hash) {
        fclose(fp);
        return BTD_ERR_REF_HASH;
    }

    /* frame_type == PRED이면 증발 시뮬레이션 필요 */
    int is_pred = (hdr.frame_type == BTD_FRAME_PRED);
    uint32_t evap_rounds = 0;

    if (is_pred) {
        /*
         * 먼저 evap_rounds TLV를 읽어야 한다.
         * 이후 canvas에 증발 시뮬레이션 적용.
         */
        BtDeltaTlv tlv;
        if (fread(&tlv, sizeof(tlv), 1, fp) != 1) { fclose(fp); return BTD_ERR_IO; }

        if (tlv.type == BTD_SEC_EVAP_ROUNDS && tlv.len == sizeof(uint32_t)) {
            if (fread(&evap_rounds, sizeof(uint32_t), 1, fp) != 1) {
                fclose(fp); return BTD_ERR_IO;
            }
        } else {
            /* evap_rounds 섹션이 없으면 raw delta처럼 처리 */
            fseek(fp, -(long)sizeof(tlv), SEEK_CUR);
            is_pred = 0;
        }
    }

    if (is_pred && evap_rounds > 0) {
        /* canvas에 증발 시뮬레이션 적용 (in-place) */
        for (uint32_t r = 0; r < evap_rounds; r++) {
            btc_evaporate(canvas);
        }
    }

    /* 잔차 섹션 읽기 + 적용 (raw delta와 동일 로직) */
    while (!feof(fp)) {
        BtDeltaTlv tlv;
        if (fread(&tlv, sizeof(tlv), 1, fp) != 1) break;

        if (tlv.type == BTD_SEC_ADDED || tlv.type == BTD_SEC_MODIFIED) {
            uint32_t count = tlv.len / sizeof(BtDeltaCell);
            for (uint32_t i = 0; i < count; i++) {
                BtDeltaCell dc;
                if (fread(&dc, sizeof(dc), 1, fp) != 1) break;
                uint32_t idx = (uint32_t)dc.y * BTC_W + (uint32_t)dc.x;
                if (idx >= BTC_TOTAL) continue;
                int was_empty = (canvas->cells[idx].G == 0);
                canvas->cells[idx].A = dc.A;
                canvas->cells[idx].B = dc.B;
                canvas->cells[idx].G = dc.G;
                canvas->cells[idx].R = dc.R;
                canvas->cells[idx].pad = dc.pad;
                if (was_empty && dc.G > 0) canvas->used++;
                else if (!was_empty && dc.G == 0) canvas->used--;
            }
        } else if (tlv.type == BTD_SEC_REMOVED) {
            uint32_t count = tlv.len / sizeof(BtDeltaRemoved);
            for (uint32_t i = 0; i < count; i++) {
                BtDeltaRemoved dr;
                if (fread(&dr, sizeof(dr), 1, fp) != 1) break;
                uint32_t idx = (uint32_t)dr.y * BTC_W + (uint32_t)dr.x;
                if (idx >= BTC_TOTAL) continue;
                if (canvas->cells[idx].G > 0) canvas->used--;
                memset(&canvas->cells[idx], 0, sizeof(BtcCell));
            }
        } else {
            /* 미지 섹션 건너뜀 */
            fseek(fp, tlv.len, SEEK_CUR);
        }
    }

    fclose(fp);
    return BTD_OK;
}
