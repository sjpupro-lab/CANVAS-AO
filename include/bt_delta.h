/*
 * bt_delta.h — Phase D1: DELTA 프레임 (변화분만 저장)
 *
 * KEYFRAME = 전체 지형 스냅샷 (bt_stream.h)
 * DELTA    = 이전 대비 변화만 기록 (H.264 P-frame 개념)
 *
 * 포맷: bt_brain_NNNN.delta
 *   BtDeltaHeader (40B)
 *   SEC_ADDED    (신규 셀)
 *   SEC_MODIFIED (변경 셀)
 *   SEC_REMOVED  (소멸 셀)
 */
#ifndef BT_DELTA_H
#define BT_DELTA_H

#include <stdint.h>
#include "bt_canvas.h"

#define BTD_MAGIC   0x42544446u  /* 'BTDF' */
#define BTD_VERSION 0x00010000u

/* 프레임 타입 */
#define BTD_FRAME_DELTA     2   /* Raw delta */
#define BTD_FRAME_PRED      3   /* Predicted delta (Phase D) */

/* 결과 코드 */
#define BTD_OK             0
#define BTD_ERR_REF_HASH  -1   /* DK-CHAIN: ref_hash 불일치 */
#define BTD_ERR_CRC       -2
#define BTD_ERR_IO        -3
#define BTD_ERR_ALLOC     -4

/* 섹션 타입 */
#define BTD_SEC_ADDED    0x01u
#define BTD_SEC_MODIFIED 0x02u
#define BTD_SEC_REMOVED  0x03u

/* ── 헤더 (40B) ── */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;           /* BTD_MAGIC */
    uint32_t version;         /* BTD_VERSION */
    uint32_t frame_type;      /* 2 = DELTA */
    uint32_t frame_index;     /* GOP 내 번호 */
    uint32_t ref_frame_hash;  /* 참조 KEYFRAME의 canvas_hash (FNV-1a) */
    uint32_t delta_cells;     /* 변화된 총 셀 수 */
    uint64_t tick_start;
    uint64_t tick_end;
    uint32_t header_crc;      /* CRC32[0..35] */
} BtDeltaHeader;  /* 40 bytes */

/* 추가/변경 셀 (14B, BtStreamCell과 동일) */
typedef struct {
    uint16_t x, y;
    uint32_t A;
    uint8_t  B, G, R, pad;
} BtDeltaCell;  /* 14 bytes */

/* 소멸 셀 (4B, 좌표만) */
typedef struct {
    uint16_t x, y;
} BtDeltaRemoved;  /* 4 bytes */

/* TLV 섹션 헤더 (6B) */
typedef struct {
    uint8_t  type;   /* BTD_SEC_* */
    uint32_t len;    /* 데이터 바이트 수 */
    uint8_t  pad;
} BtDeltaTlv;  /* 6 bytes */
#pragma pack(pop)

/* ── API ── */

/*
 * bt_delta_save: 두 캔버스 비교 → 변화 셀만 저장
 *
 * before:      이전 KEYFRAME 상태
 * after:       현재 캔버스 상태
 * path:        출력 .delta 파일 경로
 * frame_index: GOP 내 순번
 * ref_hash:    before의 btc_canvas_hash()
 * tick_start/end: 이 DELTA가 커버하는 tick 범위
 */
int bt_delta_save(const BtCanvas *before,
                  const BtCanvas *after,
                  const char     *path,
                  int             frame_index,
                  uint32_t        ref_hash,
                  uint64_t        tick_start,
                  uint64_t        tick_end);

/*
 * bt_delta_apply: .delta 파일 → 캔버스에 패치
 *
 * DK-CHAIN: ref_frame_hash vs 현재 canvas_hash 비교.
 * 불일치 → BTD_ERR_REF_HASH 즉시 반환.
 */
int bt_delta_apply(BtCanvas *canvas, const char *path);

/*
 * bt_delta_chain_apply: 디렉토리의 DELTA 체인 순서 적용
 *
 * dir:   .delta 파일 디렉토리
 * count: 적용할 DELTA 수
 */
int bt_delta_chain_apply(BtCanvas *canvas, const char *dir, int count);

/*
 * bt_delta_chain_seek: 특정 tick까지 DELTA 체인 재생
 */
int bt_delta_chain_seek(BtCanvas *canvas, const char *dir, uint64_t target_tick);

/*
 * bt_delta_info: 헤더만 읽기
 */
int bt_delta_info(const char *path, BtDeltaHeader *out_hdr);

/* ── Phase D: Predicted Delta ──────────────────────── */
/*
 * 핵심 원리:
 *   Raw delta    = diff(before, after)                        → 모든 변화
 *   Predicted delta = diff(simulate_evaporate(before, N), after) → 학습 잔차만
 *
 * 증발은 결정론적이므로 디코더가 before + evap_rounds로
 * predicted 상태를 복원한 후 잔차만 적용.
 *
 * 헤더에 evap_rounds 기록 (frame_type=3=PRED).
 * 기존 Raw delta와 파일 포맷 동일. frame_type만 다름.
 */

/*
 * bt_delta_save_pred: 예측 기반 DELTA 저장
 *
 * before → simulate_evaporate(N) → predicted
 * diff(predicted, after) → 잔차만 저장
 *
 * evap_rounds: 증발 시뮬레이션 라운드 (헤더의 reserved 영역에 기록)
 *              0이면 raw delta와 동일
 *
 * 반환: BTD_OK 또는 에러 코드
 */
int bt_delta_save_pred(const BtCanvas *before,
                       const BtCanvas *after,
                       const char     *path,
                       int             frame_index,
                       uint32_t        ref_hash,
                       uint64_t        tick_start,
                       uint64_t        tick_end,
                       uint32_t        evap_rounds);

/*
 * bt_delta_apply_pred: 예측 기반 DELTA 적용
 *
 * 1. 헤더에서 evap_rounds 읽기
 * 2. canvas를 simulate_evaporate(N)
 * 3. 잔차 적용
 *
 * frame_type == BTD_FRAME_PRED이면 예측 경로,
 * BTD_FRAME_DELTA이면 기존 raw 경로 (자동 판별).
 *
 * 반환: BTD_OK 또는 에러 코드
 */
int bt_delta_apply_pred(BtCanvas *canvas, const char *path);

/*
 * bt_delta_pred_info: Predicted delta 통계
 *
 * raw_cells:  Raw delta 셀 수 (예측 안 했을 때)
 * pred_cells: Predicted delta 셀 수 (예측 후)
 * evap_rounds: 사용된 증발 라운드
 * savings_pct: (1 - pred_cells/raw_cells) × 100
 */
typedef struct {
    uint32_t raw_cells;
    uint32_t pred_cells;
    uint32_t evap_rounds;
    uint32_t savings_pct_x10;  /* (1 - pred_cells/raw_cells) × 1000 — DK-2 */
} BtDeltaPredStats;

#endif /* BT_DELTA_H */
