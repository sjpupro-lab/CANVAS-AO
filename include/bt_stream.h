/*
 * bt_stream.h — Phase D: BT Brain Stream Format
 *
 * 물의 흐름을 기록한다.
 * 노이즈 포함 상태(키프레임) + 증발 과정(델타) = 영상.
 *
 * 포맷:
 *   BtStreamHeader (44B)
 *   Section 1: NGRAM_CELLS (활성 셀만, 14B/cell)
 *   Section 2: METADATA (학습 통계 + 윈도우)
 *   Section 3: COMPRESS_STATS (optional)
 *
 * 128MB 캔버스 → 활성 셀만 저장 → ~5-10% 크기
 */
#ifndef BT_STREAM_H
#define BT_STREAM_H

#include <stdint.h>
#include <stdio.h>
#include "bt_canvas.h"

#define BTS_MAGIC       0x4254534Bu  /* 'BTSK' */
#define BTS_VERSION     0x00010000u

/* ── 프레임 타입 ── */
#define BTS_KEYFRAME    1
#define BTS_DELTA       2

/* ── 섹션 타입 ── */
#define BTS_SEC_CELLS   0x10u   /* 활성 셀 덤프 */
#define BTS_SEC_META    0x20u   /* 메타데이터 */
#define BTS_SEC_STATS   0x30u   /* 압축 통계 (optional) */

/* ── 헤더 (44B, CVP와 동일 크기) ── */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;              /* BTS_MAGIC */
    uint32_t version;            /* BTS_VERSION */
    uint32_t frame_type;         /* KEYFRAME or DELTA */
    uint32_t active_cells;       /* used count at save time */
    uint32_t total_trained;      /* total bytes trained */
    uint32_t canvas_hash;        /* FNV-1a of all active cells */
    uint32_t section_count;      /* TLV sections following */
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t header_crc;         /* CRC32 of bytes [0..39] */
} BtStreamHeader;  /* 44 bytes */

/* ── TLV 섹션 헤더 (10B) ── */
typedef struct {
    uint16_t type;
    uint32_t len;
    uint32_t crc;
} BtStreamTlv;  /* 10 bytes */

/* ── 셀 직렬화 (14B/cell) ── */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint32_t A;
    uint8_t  B;
    uint8_t  G;
    uint8_t  R;
    uint8_t  pad;
} BtStreamCell;  /* 14 bytes */

/* ── 메타데이터 섹션 (48B) ── */
typedef struct {
    uint32_t total_trained;
    uint32_t total_predicted;
    uint32_t used;
    uint32_t collisions;
    uint8_t  window[32];
    uint8_t  win_len;
    uint8_t  reserved[3];
} BtStreamMeta;  /* 48 bytes */
#pragma pack(pop)

/* ── 결과 코드 ── */
#define BTS_OK          0
#define BTS_ERR_MAGIC   -1
#define BTS_ERR_CRC     -2
#define BTS_ERR_IO      -3
#define BTS_ERR_VERSION -4
#define BTS_ERR_ALLOC   -5

/* ── Level 2: 학습 애니메이션 프레임 API ── */

/*
 * bt_stream_export_frame: 현재 캔버스 상태 → BMP 프레임
 *
 * 학습 중 주기적으로 호출하면 학습 과정 애니메이션 생성 가능.
 * 내부적으로 btcr_export_frame() 호출.
 *
 * dir:       출력 디렉토리 (NULL이면 "frames" 사용)
 * frame_idx: 프레임 번호 → "frames/frame_0001.bmp"
 * 반환: 0=성공, -1=실패
 */
int bt_stream_export_frame(const BtCanvas *c, const char *dir, int frame_idx);

/*
 * bt_stream_export_delta: 두 상태 차이 → 델타 BMP
 *
 * 증발/학습 프로세스의 변화를 색상으로 시각화.
 * 노랑/초록 = 강화된 패턴, 파랑/보라 = 증발된 패턴.
 */
int bt_stream_export_delta(const BtCanvas *before, const BtCanvas *after,
                            const char *dir, int frame_idx);

/* ── API ── */

/*
 * bt_stream_save: BtCanvas → .stream 파일 (키프레임)
 *
 * 활성 셀만 저장. 128MB → ~수 MB.
 * 노이즈 포함 = 가능성의 전체 지형 보존.
 *
 * 반환: BTS_OK 또는 에러 코드
 */
int bt_stream_save(const BtCanvas *c, const char *path);

/*
 * bt_stream_load: .stream 파일 → BtCanvas 복원
 *
 * 키프레임 읽기 → 활성 셀 배치 → 즉시 학습/추론 가능.
 * CRC 검증 실패 시 에러 반환.
 *
 * 반환: BTS_OK 또는 에러 코드
 */
int bt_stream_load(BtCanvas *c, const char *path);

/*
 * bt_stream_validate: 파일 무결성 검증 (로드하지 않음)
 *
 * 헤더 CRC + 각 섹션 CRC 확인.
 * 반환: BTS_OK 또는 에러 코드
 */
int bt_stream_validate(const char *path);

/*
 * bt_stream_info: 헤더 정보만 읽기 (빠른 조회)
 */
int bt_stream_info(const char *path, BtStreamHeader *hdr);

#endif /* BT_STREAM_H */
