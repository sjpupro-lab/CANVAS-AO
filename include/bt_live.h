/*
 * bt_live.h — Phase D2: 살아있는 AI 세션
 *
 * 로드하면 중단된 지점의 컨텍스트 그대로 이어진다.
 * bt_delta.c의 DELTA 체인 + KEYFRAME을 통합 관리.
 *
 * 파일 포맷: bt_session.btlive
 *   BtLiveHeader
 *   [KEYFRAME 바이트 (bt_stream 포맷)]
 *   [DELTA 파일들은 delta_dir에 별도 저장]
 */
#ifndef BT_LIVE_H
#define BT_LIVE_H

#include <stdint.h>
#include "bt_canvas.h"
#include "bt_pattern.h"
#include "bt_stream.h"
#include "bt_delta.h"

#define BTL_MAGIC   0x4254534Cu  /* 'BTSL' */
#define BTL_VERSION 0x00010000u

#define BTL_OK          0
#define BTL_ERR_CRC    -1
#define BTL_ERR_MAGIC  -2
#define BTL_ERR_IO     -3
#define BTL_ERR_ALLOC  -4

#define BTL_GOP_MAX_BYTES  4096  /* DELTA flush 간격 */
#define BTL_EVAP_INTERVAL   512  /* DK-EVAP: 증발 간격 */

/* ── 헤더 ── */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;              /* BTL_MAGIC */
    uint32_t version;            /* BTL_VERSION */
    uint8_t  session_id[16];     /* 고유 식별자 (랜덤 16바이트) */
    uint64_t created_at;         /* 생성 시각 (Unix timestamp 또는 tick) */
    uint64_t saved_at;           /* 마지막 저장 시각 */
    uint8_t  domain;             /* 0=대화 1=금융 2=코드 */
    uint8_t  reserved[7];
    uint32_t canvas_hash;        /* 저장 시점 캔버스 해시 */
    uint32_t delta_count;        /* 누적 DELTA 수 */
    uint32_t total_trained;      /* 총 학습 바이트 */
    uint8_t  context_len;        /* 현재 윈도우 길이 */
    uint8_t  context_window[32]; /* 마지막 문맥 */
    uint8_t  pad[3];
    uint32_t header_crc;         /* CRC32 */
} BtLiveHeader;  /* 96 bytes */
#pragma pack(pop)

/* ── 세션 구조체 ── */
typedef struct {
    BtCanvas    *canvas;
    BtEngine     engine;
    BtLiveHeader header;

    /* 런타임 상태 */
    uint64_t     current_tick;
    uint32_t     bytes_since_evap;
    uint32_t     bytes_since_checkpoint;

    /* DELTA 관리 */
    BtCanvas    *prev_snapshot;   /* 이전 KEYFRAME 스냅샷 (heap) */
    uint32_t     prev_hash;       /* prev_snapshot의 canvas_hash */
    int          delta_count;
    char         delta_dir[256];

    /* 출력 */
    uint8_t      last_predicted;
    uint32_t     last_energy;
} BtLiveSession;

/* ── API ── */

/*
 * bt_live_init: 세션 초기화
 *
 * canvas:    외부 소유 BtCanvas (calloc으로 할당)
 * delta_dir: DELTA 파일 저장 디렉토리
 * domain:    0=대화, 1=금융, 2=코드
 */
void bt_live_init(BtLiveSession *s, BtCanvas *c,
                  const char *delta_dir, uint8_t domain);

/*
 * bt_live_feed: 입력 1바이트 → 학습 + 예측 + 감쇠 + DELTA
 *
 * 1. bt_engine_train(byte)
 * 2. 512바이트마다 btc_evaporate() (DK-EVAP)
 * 3. 4096바이트마다 bt_live_checkpoint() (DELTA flush)
 * 4. bt_engine_decide() → 예측
 * 5. current_tick++
 *
 * 반환: 예측 바이트
 */
uint8_t bt_live_feed(BtLiveSession *s, uint8_t input_byte);

/*
 * bt_live_checkpoint: 현재 상태 → DELTA 저장 + 새 스냅샷
 */
int bt_live_checkpoint(BtLiveSession *s);

/*
 * bt_live_save: 세션 전체를 .btlive 파일로 저장
 *
 * [BtLiveHeader][KEYFRAME 바이트]
 * DELTA 파일들은 delta_dir에 이미 있음
 */
int bt_live_save(const BtLiveSession *s, const char *path);

/*
 * bt_live_load: .btlive 파일에서 세션 복원
 *
 * 복원 후 bt_live_feed() 즉시 가능
 */
int bt_live_load(BtLiveSession *s, const char *path);

/*
 * bt_live_free: 내부 리소스 해제
 */
void bt_live_free(BtLiveSession *s);

/* ── Phase D: Predicted Checkpoint ─────────────────── */

/*
 * bt_live_checkpoint_pred: 예측 기반 DELTA 체크포인트
 *
 * 기존 bt_live_checkpoint()는 raw delta를 저장.
 * 이 함수는 predicted delta를 저장:
 *   diff(simulate_evap(prev, N), current) = 학습 잔차만
 *
 * evap_rounds: 증발 시뮬레이션 라운드 (0이면 raw 폴백)
 *
 * 타이밍:
 *   bt_live_feed() 내부에서 GOP (4096바이트) 간격마다 호출.
 *   이 구간 동안 btc_evaporate()가 512바이트마다 호출됨.
 *   → evap_rounds = 4096/512 = 8이 최적값.
 *
 * 반환: BTL_OK 또는 에러 코드
 */
int bt_live_checkpoint_pred(BtLiveSession *s, uint32_t evap_rounds);

/*
 * bt_live_feed_pred: 예측 기반 feed (checkpoint_pred 사용)
 *
 * bt_live_feed()와 동일하되, DELTA checkpoint를
 * bt_live_checkpoint_pred(evap_rounds)로 수행.
 *
 * evap_rounds: 증발 시뮬레이션 라운드
 * 반환: 예측 바이트
 */
uint8_t bt_live_feed_pred(BtLiveSession *s, uint8_t input_byte,
                          uint32_t evap_rounds);

/* ── SJQ-B: 데이터 압축 (예측 잔차 인코딩) ──────── */
/*
 * 캔버스가 다음 바이트를 예측 → 맞으면 저장 안 함 → 틀리면 XOR 교정값만 저장.
 * 인코더/디코더 모두 빈 캔버스에서 시작 (Stateless) → 동기화 보장.
 *
 * 포맷: .sjqb
 *   SjqbHeader (24B)
 *   SjqbCorrection × correction_count
 */

#define SJQB_MAGIC   0x534A5142u  /* 'SJQB' */
#define SJQB_VERSION 0x00010000u

#define SJQB_OK          0
#define SJQB_ERR_IO     -1
#define SJQB_ERR_ALLOC  -2
#define SJQB_ERR_CRC    -3
#define SJQB_ERR_MAGIC  -4

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;              /* SJQB_MAGIC */
    uint32_t version;            /* SJQB_VERSION */
    uint32_t original_len;       /* 원본 데이터 바이트 수 */
    uint32_t correction_count;   /* 교정값 개수 */
    uint32_t canvas_hash;        /* 인코딩 후 캔버스 해시 */
    uint32_t header_crc;         /* CRC32[0..19] */
} SjqbHeader;  /* 24 bytes */

typedef struct {
    uint32_t offset;             /* 원본에서의 위치 */
    uint8_t  correction;         /* XOR 교정값 (actual ^ predicted) */
} SjqbCorrection;  /* 5 bytes */
#pragma pack(pop)

/* 인코딩 결과 통계 */
typedef struct {
    uint32_t original_len;       /* 원본 크기 */
    uint32_t correction_count;   /* 교정값 수 */
    uint32_t file_size;          /* .sjqb 파일 크기 */
    uint32_t hit_rate_x1000;     /* 예측 적중률 ×1000 (예: 996 = 99.6%) — DK-2 */
    uint32_t comp_ratio_x100;   /* original_len * 100 / file_size — DK-2 */
} SjqbStats;

/*
 * bt_sjqb_encode: 입력 데이터 → .sjqb 파일
 *
 * Stateless 모드: 빈 캔버스에서 시작.
 * 매 바이트마다:
 *   1. bt_engine_decide() → 예측
 *   2. actual XOR predicted → 0이면 skip, 아니면 교정값 저장
 *   3. bt_engine_train(actual) → 모델 갱신
 *   4. DK-EVAP every 512B
 *
 * s:     세션 (빈 캔버스로 초기화된 상태)
 * data:  입력 데이터
 * len:   입력 길이
 * path:  출력 .sjqb 파일 경로
 * stats: (optional) 통계 출력
 *
 * 반환: SJQB_OK 또는 에러 코드
 */
int bt_sjqb_encode(BtLiveSession *s,
                   const uint8_t *data, size_t len,
                   const char *path,
                   SjqbStats *stats);

/*
 * bt_sjqb_decode: .sjqb 파일 → 원본 데이터 복원
 *
 * Stateless 모드: 빈 캔버스에서 시작.
 * 매 바이트마다:
 *   1. bt_engine_decide() → 예측
 *   2. 교정값이 있으면 predicted XOR correction, 없으면 predicted 그대로
 *   3. bt_engine_train(복원된 바이트) → 모델 갱신 (인코더와 동기)
 *   4. DK-EVAP every 512B
 *
 * s:       세션 (빈 캔버스로 초기화된 상태)
 * path:    입력 .sjqb 파일 경로
 * out:     출력 버퍼 (호출자 할당, original_len 이상)
 * out_len: 복원된 데이터 길이 (출력)
 *
 * 반환: SJQB_OK 또는 에러 코드
 */
int bt_sjqb_decode(BtLiveSession *s,
                   const char *path,
                   uint8_t *out, size_t *out_len);

/*
 * bt_sjqb_info: .sjqb 헤더만 읽기 (빠른 조회)
 */
int bt_sjqb_info(const char *path, SjqbHeader *out_hdr);

/* ── SJQ-B v2: RLE 교정값 포맷 ────────────────────── */
/*
 * v1 교정값: 5B/correction (offset 4B + correction 1B)
 *   → 원본이 작거나 적중률이 낮으면 원본보다 커짐
 *
 * v2 비트맵+RLE 포맷:
 *   비트맵: 각 바이트 위치의 적중/미스 (1bit/byte)
 *   교정값: 미스 위치의 XOR 값만 (1B/miss)
 *   → 적중률 50%이면 원본의 ~62.5% (bitmap 12.5% + corrections 50%)
 *   → 적중률 90%이면 원본의 ~22.5% (bitmap 12.5% + corrections 10%)
 *
 * 추가: RLE로 비트맵 압축 (연속 적중 구간을 run-length로)
 */

#define SJQB_V2_MAGIC   0x534A5132u  /* 'SJQ2' */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;              /* SJQB_V2_MAGIC */
    uint32_t version;            /* 0x00020000 */
    uint32_t original_len;       /* 원본 바이트 수 */
    uint32_t correction_count;   /* 미스 수 */
    uint32_t bitmap_rle_len;     /* RLE 압축된 비트맵 크기 */
    uint32_t canvas_hash;
    uint32_t header_crc;
} SjqbV2Header;  /* 28 bytes */
#pragma pack(pop)

/*
 * bt_sjqb_encode_v2: RLE 비트맵 + 교정값만 저장
 *
 * 비트맵 RLE 인코딩:
 *   0x00~0x7F: 다음 N+1 바이트가 적중 (run of hits)
 *   0x80~0xFF: 다음 (N-0x80)+1 바이트가 미스 (run of misses)
 *
 * 파일 구조:
 *   [SjqbV2Header][RLE bitmap][corrections only]
 */
int bt_sjqb_encode_v2(BtLiveSession *s,
                      const uint8_t *data, size_t len,
                      const char *path,
                      SjqbStats *stats);

/*
 * bt_sjqb_decode_v2: RLE 비트맵 디코딩 + 교정값 적용
 */
int bt_sjqb_decode_v2(BtLiveSession *s,
                      const char *path,
                      uint8_t *out, size_t *out_len);

/* ── v6f 인코딩 (가격 → 바이트) ──────────────────── */
/*
 * 가격 변화율 ±5% → 0~255 바이트
 *   -5% → 0,  0% → 128,  +5% → 255
 *   해상도: 10% / 256 = 0.039%/step
 *
 * DK-2 준수: 정수 연산만 사용
 */

/*
 * v6f_encode_price_change: 변화율 → 바이트 (정수 연산)
 *
 * pct_x100: 변화율 × 100 (정수, 예: +2.5% → 250)
 * 반환: 0~255
 */
static inline uint8_t v6f_encode_price_change(int32_t pct_x100) {
    /* clamp to ±500 (= ±5.00%) */
    if (pct_x100 < -500) pct_x100 = -500;
    if (pct_x100 >  500) pct_x100 =  500;
    /* map [-500, +500] → [0, 255] */
    int32_t val = (pct_x100 + 500) * 255 / 1000;
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    return (uint8_t)val;
}

/*
 * v6f_decode_price_change: 바이트 → 변화율 × 100 (정수)
 */
static inline int32_t v6f_decode_price_change(uint8_t byte) {
    return (int32_t)byte * 1000 / 255 - 500;
}

/*
 * v6f_encode_csv_prices: CSV close 가격 배열 → v6f 바이트 배열
 *
 * prices_x100: 가격 × 100 (정수, 예: $42280.14 → 4228014)
 * count:       가격 수
 * out:         출력 바이트 배열 (호출자 할당, count-1 크기)
 *
 * 반환: 인코딩된 바이트 수 (= count - 1)
 */
int v6f_encode_csv_prices(const int64_t *prices_x100, int count,
                          uint8_t *out);

/* ── 변동성 예측 (방향이 아닌 크기) ──────────────── */
/*
 * v6f 바이트에서 변동성 = |byte - 128|
 *   128 = 무변동, 0 or 255 = ±5% 최대 변동
 *
 * v6f_volatility: 단일 바이트의 변동성 (0~127)
 */
static inline uint8_t v6f_volatility(uint8_t v6f_byte) {
    int diff = (int)v6f_byte - 128;
    return (uint8_t)(diff < 0 ? -diff : diff);
}

/*
 * v6f_encode_volatility: 가격 변화율 시퀀스 → 변동성 시퀀스
 *
 * 방향을 제거하고 크기만 남김.
 * 변동성 예측은 방향 예측보다 본질적으로 쉬움:
 *   "내일 큰 움직임이 있을 것이다" > "내일 오를 것이다"
 */
static inline void v6f_encode_volatility(const uint8_t *v6f, int len,
                                         uint8_t *vol_out) {
    for (int i = 0; i < len; i++)
        vol_out[i] = v6f_volatility(v6f[i]);
}

/* ── 교정값 클러스터링 (이상 감지) ──────────────── */
/*
 * 교정값이 집중된 구간 = 캔버스의 예측이 갑자기 틀림 = 이상 이벤트
 *
 * SjqbAnomaly: 연속 미스 구간
 */
typedef struct {
    uint32_t start;     /* 시작 offset */
    uint32_t length;    /* 연속 미스 길이 */
    uint32_t intensity; /* 교정값 합계 (이상 강도) */
} SjqbAnomaly;

/*
 * sjqb_find_anomalies: 교정값 클러스터에서 이상 구간 검출
 *
 * min_run: 최소 연속 미스 길이 (이 이상이면 이상으로 판정)
 * out:     출력 배열 (호출자 할당)
 * max_out: 최대 출력 수
 *
 * 반환: 검출된 이상 수
 */
int sjqb_find_anomalies(const char *sjqb_v2_path,
                        int min_run,
                        SjqbAnomaly *out, int max_out);

#endif /* BT_LIVE_H */
