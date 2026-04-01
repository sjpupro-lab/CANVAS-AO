/*
 * bt_canvas.c — BT ↔ Canvas 브릿지
 *
 * n-gram 엔트리가 캔버스 셀로 살아간다.
 * 저장하면 BMP 이미지. 로드하면 같은 AI.
 *
 * 매핑:
 *   (x, y) = hash(ctx, next_byte) → 캔버스 좌표
 *   Cell.A = ctx 해시 (충돌 검증용)
 *   Cell.B = ctx_len (depth, 1~8)
 *   Cell.G = freq 밝기 (log2 스케일, 0~255)
 *   Cell.R = next_byte (예측 바이트)
 *
 * 1024×1024 = 1M 셀 → 최대 ~800K 엔트리 (open addressing)
 * 충돌: linear probing (캔버스에서 옆 칸으로 이동)
 */
#ifndef BT_CANVAS_H
#define BT_CANVAS_H

#include <stdint.h>

#define BTC_W 4096
#define BTC_H 4096
#define BTC_TOTAL (BTC_W * BTC_H)

/* 캔버스 셀 (CanvasOS Cell과 동일 레이아웃) */
typedef struct {
    uint32_t A;
    uint8_t  B;
    uint8_t  G;
    uint8_t  R;
    uint8_t  pad;
} BtcCell;

/* 캔버스 AI */
typedef struct {
    BtcCell  cells[BTC_TOTAL];
    uint32_t used;           /* 사용 중인 셀 수 */
    uint32_t collisions;     /* 충돌 횟수 */

    /* 문맥 윈도우 */
    uint8_t  window[32];
    int      win_len;

    /* 통계 */
    uint32_t total_trained;
    uint32_t total_predicted;
} BtCanvas;

/* ── API ──────────────────────────────────────────── */

void    btc_init(BtCanvas *c);

/* 학습: 바이트 1개 → 캔버스 셀에 누산 */
void    btc_train(BtCanvas *c, uint8_t byte);

/* 예측: P(byte | context), Q16.16 고정소수점 반환 — DK-2 준수
 * 256 = 1/256 (균등), 65536 = 100% */
uint32_t btc_predict(BtCanvas *c, uint8_t byte);

/* BMP 저장/로드 (8바이트 셀 → 2×RGBA 픽셀) */
int     btc_save_bmp(const BtCanvas *c, const char *path);
int     btc_load_bmp(BtCanvas *c, const char *path);

/* 통계 */
void    btc_stats(const BtCanvas *c);

/* 검증: 저장 → 로드 → 동일성 확인 */
int     btc_verify_roundtrip(BtCanvas *c, const char *path);

/* ── 캔버스 해시 (FNV-1a, 활성 셀만) ────────────── */
/* bt_delta, bt_stream 등에서 무결성 검증용 */
uint32_t btc_canvas_hash(const BtCanvas *c);

/* ── Phase C: 캔버스 압축 ─────────────────────────── */

/*
 * btc_evaporate: 자연 감쇠 (물의 원리)
 *
 * 모든 확률은 존재한다. 물은 모든 경로로 흐른다.
 * 하지만 얕은 수로는 증발한다 — 외부가 지우는 게 아니라, 스스로 마른다.
 *
 * 캔버스의 일부를 샘플링하여 freq를 1 감소시킨다.
 * freq=0이 된 셀은 자연 소멸 (수로가 마름).
 * 자주 강화되는 패턴(깊은 수로)은 살아남는다.
 *
 * bt_live_feed() 내부에서 매 512바이트마다 호출 (DK-EVAP).
 * 외부 임계값(min_freq) 불필요 — 물이 스스로 결정한다.
 */
void btc_evaporate(BtCanvas *c);

/* 압축 통계 */
typedef struct {
    uint32_t cells_before;   /* 압축 전 활성 셀 */
    uint32_t cells_after;    /* 압축 후 활성 셀 */
    uint32_t pruned;         /* 제거된 희소 셀 */
    uint32_t merged;         /* 병합된 셀 */
    uint32_t rescaled;       /* G 재스케일 셀 */
    uint32_t ratio_x100;     /* cells_before * 100 / cells_after — DK-2 */
} BtcCompressStats;

/*
 * btc_compress: 캔버스 압축 (3단계)
 *
 * 1. 희소 셀 회수: freq < min_freq인 셀을 0으로 클리어
 *    → 한 번밖에 안 나온 패턴은 잡음. 제거하면 공간 확보.
 *
 * 2. 공유 루트 병합: 같은 (ctx_hash, depth)의 약한 후보를 강한 후보에 에너지 이관
 *    → "the"→{a,b,c,d,...}에서 freq<5인 후보의 에너지를 상위에 합산
 *    → 투명 필름처럼 겹쳐서 밝은 곳만 남김
 *
 * 3. G 재스케일: 포화(G=255) 방지를 위해 모든 G를 비례 축소
 *    → 학습 많이 하면 모든 셀이 255에 붙는 문제 해결
 *
 * min_freq: 보존할 최소 빈도 (권장: 2~5)
 * 반환: 압축 통계
 */
BtcCompressStats btc_compress(BtCanvas *c, uint32_t min_freq);

/* ── Multi-Lane 학습/예측 ────────────────────────── */

#define BTC_MAX_LANES 8

/*
 * Lane 컨텍스트: 각 Lane은 독립 윈도우를 가진다.
 * 동일 캔버스에 lane_id를 해시에 혼합하여 독립 n-gram 공간 생성.
 * lane_id=0이면 기존 btc_train()과 동일 (후방 호환).
 */
typedef struct {
    uint8_t window[32];
    int     win_len;
} BtcLaneCtx;

/*
 * btc_train_lane: Lane별 독립 학습
 *
 * lane_id를 해시에 혼합: ctx_hash ^= (lane_id * 0x9e3779b9u)
 * → 동일 캔버스에서 Lane별 독립 n-gram 공간
 * → Lane 0은 기존 btc_train()과 동일
 */
void btc_train_lane(BtCanvas *c, uint8_t byte, uint8_t lane_id,
                    BtcLaneCtx *lctx);

/*
 * btc_predict_lane: Lane별 예측
 */
uint32_t btc_predict_lane(BtCanvas *c, uint8_t byte, uint8_t lane_id,
                          BtcLaneCtx *lctx);

/*
 * btc_cross_boost: 두 Lane 간 에너지 교차 부스트
 *
 * Lane A와 Lane B에서 동일 바이트가 높은 에너지로 존재할 때,
 * 양쪽의 G 에너지를 boost_factor만큼 증가.
 * "두 수로가 같은 곳을 가리키면 더 깊어진다."
 *
 * 반환: 부스트된 셀 수
 */
uint32_t btc_cross_boost(BtCanvas *c,
                         uint8_t lane_a, BtcLaneCtx *ctx_a,
                         uint8_t lane_b, BtcLaneCtx *ctx_b,
                         uint32_t boost_factor);

/*
 * btc_cross_boost_candle: 캔들 단위 교차 부스트
 *
 * 바이트 수준이 아닌 캔들(의미 단위) 수준에서 교차:
 *   "가격 캔들이 상승(v6f > 128)" + "거래량 캔들이 급증(v6f > 180)"
 *   → 양쪽 Lane의 최근 n-gram 셀에 부스트
 *
 * 작동 원리:
 *   각 Lane의 최근 예측 바이트(bt_engine_decide)를 비교.
 *   양쪽이 모두 "같은 방향"이면 부스트.
 *     방향 = v6f byte > 128 (상승) or < 128 (하락)
 *     방향 일치 + 에너지가 threshold 이상 → 부스트
 *
 * threshold: 부스트 발동 최소 에너지 (0이면 항상 발동)
 * 반환: 부스트된 셀 수
 */
uint32_t btc_cross_boost_candle(BtCanvas *c,
                                uint8_t lane_a, BtcLaneCtx *ctx_a,
                                uint8_t lane_b, BtcLaneCtx *ctx_b,
                                uint32_t boost_factor,
                                uint32_t threshold);

/* ── Phase D: 예측 증발 (시뮬레이션) ─────────────── */

/*
 * btc_simulate_evaporate: 원본을 건드리지 않고 증발 결과를 예측
 *
 * 원본 캔버스에 N 라운드 증발을 적용한 결과를 dst에 기록.
 * 원본(src)은 수정하지 않는다.
 *
 * 용도: Predicted Delta 계산.
 *   Raw delta    = diff(before, after)
 *   Pred delta   = diff(simulate_evap(before, N), after)
 *
 * 증발은 결정론적이므로, 디코더가 before + N으로
 * 동일한 predicted 상태를 복원할 수 있다.
 *
 * src:    원본 캔버스 (읽기 전용)
 * dst:    결과 캔버스 (호출자가 할당, 128MB)
 * rounds: 증발 라운드 수
 *
 * 반환: dst의 활성 셀 수
 */
uint32_t btc_simulate_evaporate(const BtCanvas *src, BtCanvas *dst,
                                uint32_t rounds);

#endif /* BT_CANVAS_H */
