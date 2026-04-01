/*
 * bt_canvas.c — AI가 캔버스 위에서 산다
 *
 * 캔버스 = 해시 테이블.
 * 셀 = n-gram 엔트리.
 * BMP로 저장하면 = AI를 이미지로 저장.
 * BMP를 로드하면 = 같은 AI 복원.
 */
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/* DK-2: math.h 제거됨 — 모든 연산 정수 전용 */
#include "bt_canvas.h"

#define MAX_DEPTH 32

/* ── 해시 → 캔버스 좌표 ──────────────────────────── */
static inline uint32_t fnv(const uint8_t *d, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) { h ^= d[i]; h *= 16777619u; }
    return h;
}

static inline uint32_t cell_hash(const uint8_t *ctx, int cl, uint8_t next) {
    uint32_t h = fnv(ctx, cl);
    h ^= next; h *= 16777619u;
    return h;
}

/* 해시 → (x, y) */
static inline void hash_to_xy(uint32_t h, uint16_t *x, uint16_t *y) {
    *x = (uint16_t)(h & (BTC_W - 1));
    *y = (uint16_t)((h >> 12) & (BTC_H - 1));
}

/* (x, y) → 인덱스 */
static inline uint32_t xy_to_idx(uint16_t x, uint16_t y) {
    return (uint32_t)y * BTC_W + (uint32_t)x;
}

/* 2^(frac/16) × 256 조견표 — DK-2 정수 전용
 * frac=0: 256 (1.000), frac=4: 304 (1.189), frac=8: 362 (1.414), frac=15: 491 (1.915) */
static const uint16_t POW2_FRAC16_Q8[16] = {
    256, 267, 278, 290, 304, 317, 331, 345,
    362, 378, 395, 413, 431, 450, 470, 491
};

/* log2 분수 역조견표: val_q8 → frac (0~15)
 * POW2_FRAC16_Q8의 역변환 */
static inline uint32_t log2_frac16(uint32_t val, uint32_t base) {
    /* val / base를 Q8로 변환, 테이블에서 가장 가까운 frac 찾기 */
    uint32_t ratio_q8 = (val * 256) / base;
    for (int f = 15; f >= 0; f--) {
        if (ratio_q8 >= POW2_FRAC16_Q8[f]) return (uint32_t)f;
    }
    return 0;
}

/* freq → G (정수 log2 스케일, 밝기) — DK-2 준수
 * freq 1 → G=16, freq 2 → G=25, freq 256 → G=128, freq 65536 → G=255 */
static inline uint8_t freq_to_g(uint32_t freq) {
    if (freq == 0) return 0;
    uint32_t v = freq + 1;
    int msb = 0;
    uint32_t tmp = v;
    while (tmp > 1) { tmp >>= 1; msb++; }
    uint32_t base = (uint32_t)1 << msb;
    uint32_t frac = log2_frac16(v, base);
    uint32_t g = (uint32_t)msb * 16 + frac;
    return g > 255 ? 255 : (uint8_t)g;
}

/* G → freq 복원 — DK-2 준수 (조견표 기반)
 * G = msb*16 + frac → freq = (1 << msb) * POW2[frac] / 256 - 1
 * G < 16 (msb=0) → freq=0 (너무 약함, 증발 대상) */
static inline uint32_t g_to_freq(uint8_t g) {
    if (g == 0) return 0;
    uint32_t msb  = (uint32_t)g / 16;
    uint32_t frac = (uint32_t)g % 16;
    uint64_t val = ((uint64_t)1 << msb) * POW2_FRAC16_Q8[frac];
    uint32_t result = (uint32_t)(val / 256);
    return result > 0 ? result - 1 : 0;
}

/* ── 셀 조회 (open addressing, linear probe) ─────── */
static BtcCell* cell_find(BtCanvas *c, const uint8_t *ctx, int cl,
                          uint8_t next, uint32_t ctx_hash, int create)
{
    uint32_t h = cell_hash(ctx, cl, next);
    uint16_t x, y;
    hash_to_xy(h, &x, &y);

    /* linear probe (최대 64칸) */
    for (int probe = 0; probe < 64; probe++) {
        uint32_t idx = xy_to_idx(
            (uint16_t)((x + probe) & (BTC_W - 1)), y);
        BtcCell *cell = &c->cells[idx];

        if (cell->B == 0 && cell->G == 0 && cell->R == 0 && cell->A == 0) {
            /* 빈 셀 */
            if (!create) return NULL;
            /* 새 엔트리 생성 */
            cell->A = ctx_hash;
            cell->B = (uint8_t)cl;
            cell->G = freq_to_g(1);
            cell->R = next;
            c->used++;
            if (probe > 0) c->collisions++;
            return cell;
        }

        /* 매칭 확인: A(해시) + B(depth) + R(next) */
        if (cell->A == ctx_hash && cell->B == (uint8_t)cl && cell->R == next) {
            return cell;
        }
    }

    /* 64칸 이내 빈 셀 없음 */
    if (create) c->collisions++;
    return NULL;
}

/* ── 문맥 총빈도 (256개 후보 스캔) ─────────────────── */
static uint32_t ctx_total(BtCanvas *c, const uint8_t *ctx, int cl) {
    uint32_t ctx_h = fnv(ctx, cl);
    uint32_t total = 0;
    for (int nb = 0; nb < 256; nb++) {
        BtcCell *cell = cell_find(c, ctx, cl, (uint8_t)nb, ctx_h, 0);
        if (cell) total += g_to_freq(cell->G);
    }
    return total;
}

/* ══════════════════════════════════════════════════ */

void btc_init(BtCanvas *c) {
    memset(c, 0, sizeof(*c));
}

/*
 * btc_evaporate — 자연 감쇠 (물의 원리)
 *
 * 물은 모든 경로로 흐르지만, 얕은 수로는 증발한다.
 * 캔버스의 일부를 샘플링하여 freq를 1 감소.
 * freq=0이 되면 셀 소멸 (수로가 마름).
 *
 * 비용: O(BTC_TOTAL / 16384) ≈ 1024 셀/호출
 * 호출: btc_train 내부에서 매 256바이트마다
 */
void btc_evaporate(BtCanvas *c) {
    if (!c || c->used == 0) return;

    /*
     * stride = 전체 / 16384 → 매 호출에서 ~1024 셀 검사
     * offset = total_trained 기반 → 매번 다른 위치 시작
     * → 전체 캔버스를 ~16회 train 후 1회 순환
     */
    /*
     * 물의 원리: 건기에 모든 수로가 동시에 증발.
     *
     * 전체 캔버스 순회. 활성 셀(G>0)만 처리.
     * 비용: O(BTC_TOTAL) 이지만, compress 내부에서만 호출됨 (train 중 아님).
     */
    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        BtcCell *cell = &c->cells[i];
        if (cell->G == 0) continue;  /* 이미 빈 셀 */

        uint32_t freq = g_to_freq(cell->G);
        if (freq <= 1) {
            /* freq ≤ 1 → 수로 소멸 */
            cell->A = 0; cell->B = 0; cell->G = 0;
            cell->R = 0; cell->pad = 0;
            c->used--;
        } else {
            /* 증발: freq 1 감소 → G 재계산 — DK-2 */
            cell->G = freq_to_g(freq - 1);
        }
    }
}

void btc_train(BtCanvas *c, uint8_t byte) {
    /* 비가 수로에 흐른다 — 매칭된 셀의 freq 증가 */
    for (int n = 1; n <= c->win_len && n <= MAX_DEPTH; n++) {
        const uint8_t *ctx = c->window + (c->win_len - n);
        uint32_t ctx_h = fnv(ctx, n);
        BtcCell *cell = cell_find(c, ctx, n, byte, ctx_h, 1);
        if (cell) {
            uint32_t freq = g_to_freq(cell->G) + 1;
            cell->G = freq_to_g(freq);
        }
    }

    /* 윈도우 업데이트 */
    if (c->win_len < MAX_DEPTH) {
        c->window[c->win_len++] = byte;
    } else {
        memmove(c->window, c->window + 1, MAX_DEPTH - 1);
        c->window[MAX_DEPTH - 1] = byte;
    }
    c->total_trained++;
}

/* exp(n) 근사 테이블 — Q16 (×65536 스케일)
 * exp(1)≈2.72→178145, exp(2)≈7.39→484249, ... exp(8)≈2981→195360077
 * 오버플로 방지: Q8 스케일(×256)로 축소 */
static const uint32_t EXP_TABLE_Q8[9] = {
    256,     /* exp(0) = 1.0 */
    696,     /* exp(1) ≈ 2.718 */
    1891,    /* exp(2) ≈ 7.389 */
    5140,    /* exp(3) ≈ 20.09 */
    13968,   /* exp(4) ≈ 54.60 */
    37965,   /* exp(5) ≈ 148.4 */
    103163,  /* exp(6) ≈ 403.4 */
    280363,  /* exp(7) ≈ 1097  */
    762230   /* exp(8) ≈ 2981  */
};

/* 정수 pow(x, 4) — DK-2 준수, Q8 입출력
 * 원래 pow(conf, 20)을 사용했으나, 정수에서 x^20은 언더플로.
 * x^4로 충분한 분별력: 높은 확신도가 지수적으로 강화됨.
 * x_q8 범위: 0~65535 (Q8) */
static inline uint64_t ipow4_q8(uint32_t x_q8) {
    if (x_q8 == 0) return 0;
    uint64_t r = x_q8;
    r = (r * r) >> 8;  /* x^2 */
    r = (r * r) >> 8;  /* x^4 */
    return r;
}

/* btc_predict — DK-2 정수 전용
 * 반환: Q16.16 확률 (0 = 0%, 65536 = 100%, 256 = 1/256)
 *
 * 가중치: w = depth^2 * (af+1)^2
 * depth가 깊고 빈도가 높을수록 강한 가중치. 정수 전용. */
uint32_t btc_predict(BtCanvas *c, uint8_t byte) {
    if (c->win_len == 0) return 256; /* 1/256 in Q16 */

    int max_n = c->win_len < MAX_DEPTH ? c->win_len : MAX_DEPTH;

    uint64_t ws = 0, wt = 0;

    for (int n = 1; n <= max_n; n++) {
        const uint8_t *ctx = c->window + (c->win_len - n);
        uint32_t total = ctx_total(c, ctx, n);
        if (total == 0) continue;

        uint32_t ctx_h = fnv(ctx, n);
        BtcCell *cell = cell_find(c, ctx, n, byte, ctx_h, 0);
        uint32_t af = cell ? g_to_freq(cell->G) : 0;

        /* p_q16 = af * 65536 / total (or 256 if af==0) */
        uint32_t p_q16 = af > 0
            ? (uint32_t)((uint64_t)af * 65536 / total)
            : (uint32_t)(65536 / (total + 256));

        /* w = n^2 * (af+1)^2 — 깊은 depth + 높은 빈도 우선 */
        uint64_t w = (uint64_t)n * n * ((uint64_t)af + 1) * ((uint64_t)af + 1);
        if (w == 0) w = 1;

        ws += w * (uint64_t)p_q16;
        wt += w;
    }

    c->total_predicted++;
    if (wt == 0) return 256;

    uint32_t prob_q16 = (uint32_t)(ws / wt);
    if (prob_q16 == 0) prob_q16 = 1;
    if (prob_q16 > 65536) prob_q16 = 65536;
    return prob_q16;
}

/* ══ BMP 저장/로드 ════════════════════════════════ */
/*
 * Cell = 8바이트, BMP pixel = 4바이트(BGRA).
 * 셀 1개 = 픽셀 2개 (가로로 나란히).
 *   Pixel 1: B=cell.B, G=cell.G, R=cell.R, A=cell.pad
 *   Pixel 2: B=cell.A[0:7], G=cell.A[8:15], R=cell.A[16:23], A=cell.A[24:31]
 *
 * BMP 크기: 2048×1024, 32bit = 8MB
 */

#pragma pack(push, 1)
typedef struct {
    uint16_t type;      /* 'BM' */
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BmpFileHeader;

typedef struct {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t  xppm;
    int32_t  yppm;
    uint32_t colors;
    uint32_t important;
} BmpInfoHeader;
#pragma pack(pop)

int btc_save_bmp(const BtCanvas *c, const char *path) {
    if (!c || !path) return -1;

    int bmp_w = BTC_W * 2;  /* 셀 1개 = 2픽셀 */
    int bmp_h = BTC_H;
    int row_bytes = bmp_w * 4;
    int img_size = row_bytes * bmp_h;

    BmpFileHeader fh = {
        .type = 0x4D42, /* 'BM' */
        .size = (uint32_t)(sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + img_size),
        .offset = (uint32_t)(sizeof(BmpFileHeader) + sizeof(BmpInfoHeader))
    };
    BmpInfoHeader ih = {
        .size = sizeof(BmpInfoHeader),
        .width = bmp_w,
        .height = -bmp_h, /* top-down */
        .planes = 1,
        .bpp = 32,
        .image_size = (uint32_t)img_size
    };

    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);

    /* 셀 → 픽셀 */
    for (int y = 0; y < BTC_H; y++) {
        for (int x = 0; x < BTC_W; x++) {
            const BtcCell *cell = &c->cells[y * BTC_W + x];

            /* Pixel 1: B, G, R, pad */
            uint8_t px1[4] = { cell->B, cell->G, cell->R, cell->pad };
            fwrite(px1, 4, 1, f);

            /* Pixel 2: A as 4 bytes */
            uint8_t px2[4];
            px2[0] = (uint8_t)(cell->A & 0xFF);
            px2[1] = (uint8_t)((cell->A >> 8) & 0xFF);
            px2[2] = (uint8_t)((cell->A >> 16) & 0xFF);
            px2[3] = (uint8_t)((cell->A >> 24) & 0xFF);
            fwrite(px2, 4, 1, f);
        }
    }

    fclose(f);
    return 0;
}

int btc_load_bmp(BtCanvas *c, const char *path) {
    if (!c || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    BmpFileHeader fh;
    BmpInfoHeader ih;
    fread(&fh, sizeof(fh), 1, f);
    fread(&ih, sizeof(ih), 1, f);

    if (fh.type != 0x4D42) { fclose(f); return -2; }

    fseek(f, fh.offset, SEEK_SET);

    memset(c->cells, 0, sizeof(c->cells));
    c->used = 0;
    c->win_len = 0;

    for (int y = 0; y < BTC_H; y++) {
        for (int x = 0; x < BTC_W; x++) {
            BtcCell *cell = &c->cells[y * BTC_W + x];

            uint8_t px1[4], px2[4];
            fread(px1, 4, 1, f);
            fread(px2, 4, 1, f);

            cell->B   = px1[0];
            cell->G   = px1[1];
            cell->R   = px1[2];
            cell->pad = px1[3];
            cell->A   = (uint32_t)px2[0]
                      | ((uint32_t)px2[1] << 8)
                      | ((uint32_t)px2[2] << 16)
                      | ((uint32_t)px2[3] << 24);

            if (cell->G > 0) c->used++;
        }
    }

    fclose(f);
    return 0;
}

void btc_stats(const BtCanvas *c) {
    if (!c) return;

    uint32_t non_zero = 0;
    uint32_t max_g = 0;
    uint64_t sum_g = 0;
    uint32_t depth_count[MAX_DEPTH + 1] = {0};

    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        if (c->cells[i].G > 0) {
            non_zero++;
            sum_g += c->cells[i].G;
            if (c->cells[i].G > max_g) max_g = c->cells[i].G;
            uint8_t d = c->cells[i].B;
            if (d <= MAX_DEPTH) depth_count[d]++;
        }
    }

    /* DK-2: 정수 백분율 (×10 스케일 → 소수점 1자리) */
    uint32_t fill_x10 = (uint32_t)((uint64_t)non_zero * 1000 / BTC_TOTAL);
    uint32_t sparsity_x10 = 1000 - fill_x10;
    uint32_t raw_size = BTC_TOTAL * 8;
    uint32_t est_compressed = non_zero * 8 * 3 / 10; /* PNG 추정 30% */

    printf("\n--- BT Canvas Stats ---\n");
    printf("Canvas: %d×%d = %d cells (%d KB raw)\n",
           BTC_W, BTC_H, BTC_TOTAL, raw_size / 1024);
    printf("Used: %u cells (%u.%u%%) | Empty: %u.%u%%\n",
           non_zero, fill_x10/10, fill_x10%10, sparsity_x10/10, sparsity_x10%10);
    printf("Trained: %u bytes | Predicted: %u\n", c->total_trained, c->total_predicted);
    printf("Collisions: %u\n", c->collisions);
    printf("Max G: %u | Avg G: %u\n", max_g, non_zero > 0 ? (uint32_t)(sum_g / non_zero) : 0);
    printf("Est. compressed: ~%u KB (PNG)\n", est_compressed / 1024);
    printf("Depth distribution:\n");
    for (int d = 1; d <= MAX_DEPTH; d++) {
        if (depth_count[d] > 0)
            printf("  depth %d: %u cells\n", d, depth_count[d]);
    }
}

int btc_verify_roundtrip(BtCanvas *c, const char *path) {
    /* 저장 */
    if (btc_save_bmp(c, path) != 0) {
        printf("FAIL: save\n"); return -1;
    }

    /* 원본 해시 계산 */
    uint32_t orig_hash = 0;
    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        orig_hash ^= c->cells[i].A;
        orig_hash ^= ((uint32_t)c->cells[i].B << 16);
        orig_hash ^= ((uint32_t)c->cells[i].G << 8);
        orig_hash ^= c->cells[i].R;
        orig_hash *= 16777619u;
    }

    /* 로드 */
    BtCanvas loaded;
    btc_init(&loaded);
    if (btc_load_bmp(&loaded, path) != 0) {
        printf("FAIL: load\n"); return -2;
    }

    /* 로드된 해시 */
    uint32_t load_hash = 0;
    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        load_hash ^= loaded.cells[i].A;
        load_hash ^= ((uint32_t)loaded.cells[i].B << 16);
        load_hash ^= ((uint32_t)loaded.cells[i].G << 8);
        load_hash ^= loaded.cells[i].R;
        load_hash *= 16777619u;
    }

    if (orig_hash == load_hash) {
        printf("ROUNDTRIP OK: hash=%08X, used=%u→%u\n",
               orig_hash, c->used, loaded.used);
        return 0;
    } else {
        printf("ROUNDTRIP FAIL: orig=%08X load=%08X\n", orig_hash, load_hash);
        return -3;
    }
}

/* ══ Phase C: 캔버스 압축 (물의 원리) ════════════════ */

/*
 * btc_compress — 물의 원리 기반 압축
 *
 * 1단계: 자연 감쇠 (btc_evaporate 반복 호출)
 *   → btc_train에서 이미 주기적으로 호출됨.
 *   → 여기서는 "집중 증발" — 한 번에 여러 라운드 감쇠.
 *   → 외부 임계값(min_freq) 없음. 물이 스스로 결정.
 *
 * 2단계: 수로 합류 (공유 루트 병합)
 *   → 같은 문맥의 약한 수로가 강한 수로에 합류. 자연스러운 현상.
 *
 * 3단계: 수위 조절 (G 재스케일)
 *   → 포화 방지. 상대적 깊이 보존.
 *
 * min_freq: 후방 호환용. 0이면 자연 감쇠만 사용.
 */
BtcCompressStats btc_compress(BtCanvas *c, uint32_t min_freq) {
    BtcCompressStats stats = {0};
    if (!c) return stats;

    stats.cells_before = c->used;

    /* ── 1단계: 집중 증발 ── */
    /*
     * 물의 원리: 건기에 모든 수로가 동시에 증발.
     *
     * 라운드 수 = min_freq (후방 호환)
     * min_freq=0 → 기본 2라운드 (최적: 48% 감소, PPL 유지)
     *
     * 탐색 결과:
     *   라운드 2~5: 최적 (48~55% 감소, PPL 1.03)
     *   라운드 6+:  과격 (중간 수로 마름, PPL 급락)
     */
    uint32_t rounds = min_freq > 0 ? min_freq : 2;
    for (uint32_t r = 0; r < rounds; r++) {
        uint32_t before_round = c->used;
        btc_evaporate(c);
        stats.pruned += before_round - c->used;
    }

    /* ── 2단계: 수로 합류 (공유 루트 병합) ── */
    /*
     * 감쇠 후에도 살아남은 약한 셀이 있다면,
     * 같은 문맥의 강한 이웃에 에너지를 합류시킨다.
     * → 물이 합류하는 것은 자연스러운 현상.
     */
    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        BtcCell *cell = &c->cells[i];
        if (cell->G == 0) continue;

        uint32_t cell_freq = g_to_freq(cell->G);
        if (cell_freq > 3) continue;  /* 충분히 깊은 수로는 건드리지 않음 */

        for (int probe = 1; probe <= 8; probe++) {
            uint32_t j = (i + (uint32_t)probe) & (BTC_TOTAL - 1);
            BtcCell *neighbor = &c->cells[j];
            if (neighbor->G == 0) break;
            if (neighbor->A != cell->A || neighbor->B != cell->B) continue;
            if (g_to_freq(neighbor->G) <= cell_freq) continue;

            /* 합류: 약한 수로 → 강한 수로 */
            uint32_t nf = g_to_freq(neighbor->G) + cell_freq;
            neighbor->G = freq_to_g(nf);
            cell->A = 0; cell->B = 0; cell->G = 0;
            cell->R = 0; cell->pad = 0;
            c->used--;
            stats.merged++;
            break;
        }
    }

    /* ── 3단계: G 재스케일 ── */
    /*
     * G=255 포화 방지: max_G > 200이면 모든 G를 3/4로 축소.
     * → 새 학습을 위한 밝기 여유 확보.
     * → 기존 패턴의 상대적 밝기 순서는 보존.
     */
    uint8_t max_g = 0;
    for (uint32_t i = 0; i < BTC_TOTAL; i++) {
        if (c->cells[i].G > max_g) max_g = c->cells[i].G;
    }

    if (max_g > 200) {
        for (uint32_t i = 0; i < BTC_TOTAL; i++) {
            if (c->cells[i].G > 0) {
                c->cells[i].G = (uint8_t)((uint32_t)c->cells[i].G * 3 / 4);
                if (c->cells[i].G == 0) c->cells[i].G = 1;  /* 0 방지 */
                stats.rescaled++;
            }
        }
    }

    stats.cells_after = c->used;
    stats.ratio_x100 = stats.cells_after > 0
        ? (uint32_t)((uint64_t)stats.cells_before * 100 / stats.cells_after)
        : 0;

    return stats;
}

/* ══ Multi-Lane 학습/예측 ════════════════════════════ */

/* Lane 해시 혼합: lane_id를 FNV 해시에 주입 */
static inline uint32_t lane_mix(uint32_t ctx_hash, uint8_t lane_id) {
    if (lane_id == 0) return ctx_hash;  /* 후방 호환 */
    ctx_hash ^= (uint32_t)lane_id * 0x9e3779b9u;
    ctx_hash *= 16777619u;
    return ctx_hash;
}

void btc_train_lane(BtCanvas *c, uint8_t byte, uint8_t lane_id,
                    BtcLaneCtx *lctx)
{
    if (!c || !lctx) return;

    for (int n = 1; n <= lctx->win_len && n <= MAX_DEPTH; n++) {
        const uint8_t *ctx = lctx->window + (lctx->win_len - n);
        uint32_t ctx_h = lane_mix(fnv(ctx, n), lane_id);
        BtcCell *cell = cell_find(c, ctx, n, byte, ctx_h, 1);
        if (cell) {
            uint32_t freq = g_to_freq(cell->G) + 1;
            cell->G = freq_to_g(freq);
        }
    }

    /* 윈도우 업데이트 */
    if (lctx->win_len < MAX_DEPTH) {
        lctx->window[lctx->win_len++] = byte;
    } else {
        memmove(lctx->window, lctx->window + 1, MAX_DEPTH - 1);
        lctx->window[MAX_DEPTH - 1] = byte;
    }
    c->total_trained++;
}

/* Lane별 문맥 총빈도 */
static uint32_t ctx_total_lane(BtCanvas *c, const uint8_t *ctx, int cl,
                               uint8_t lane_id) {
    uint32_t ctx_h = lane_mix(fnv(ctx, cl), lane_id);
    uint32_t total = 0;
    for (int nb = 0; nb < 256; nb++) {
        BtcCell *cell = cell_find(c, ctx, cl, (uint8_t)nb, ctx_h, 0);
        if (cell) total += g_to_freq(cell->G);
    }
    return total;
}

/* btc_predict_lane — DK-2 정수 전용, Q16.16 반환 */
uint32_t btc_predict_lane(BtCanvas *c, uint8_t byte, uint8_t lane_id,
                          BtcLaneCtx *lctx)
{
    if (!c || !lctx || lctx->win_len == 0) return 256;

    int max_n = lctx->win_len < MAX_DEPTH ? lctx->win_len : MAX_DEPTH;
    uint64_t ws = 0, wt = 0;

    for (int n = 1; n <= max_n; n++) {
        const uint8_t *ctx = lctx->window + (lctx->win_len - n);
        uint32_t total = ctx_total_lane(c, ctx, n, lane_id);
        if (total == 0) continue;

        uint32_t ctx_h = lane_mix(fnv(ctx, n), lane_id);
        BtcCell *cell = cell_find(c, ctx, n, byte, ctx_h, 0);
        uint32_t af = cell ? g_to_freq(cell->G) : 0;

        uint32_t p_q16 = af > 0
            ? (uint32_t)((uint64_t)af * 65536 / total)
            : (uint32_t)(65536 / (total + 256));

        uint64_t w = (uint64_t)n * n * ((uint64_t)af + 1) * ((uint64_t)af + 1);
        if (w == 0) w = 1;

        ws += w * (uint64_t)p_q16;
        wt += w;
    }

    c->total_predicted++;
    if (wt == 0) return 256;

    uint32_t prob_q16 = (uint32_t)(ws / wt);
    if (prob_q16 == 0) prob_q16 = 1;
    if (prob_q16 > 65536) prob_q16 = 65536;
    return prob_q16;
}

uint32_t btc_cross_boost(BtCanvas *c,
                         uint8_t lane_a, BtcLaneCtx *ctx_a,
                         uint8_t lane_b, BtcLaneCtx *ctx_b,
                         uint32_t boost_factor)
{
    if (!c || !ctx_a || !ctx_b) return 0;
    if (boost_factor == 0) boost_factor = 1;

    uint32_t boosted = 0;

    /* 각 Lane의 최고 예측 바이트 찾기 */
    int max_n_a = ctx_a->win_len < MAX_DEPTH ? ctx_a->win_len : MAX_DEPTH;
    int max_n_b = ctx_b->win_len < MAX_DEPTH ? ctx_b->win_len : MAX_DEPTH;
    if (max_n_a == 0 || max_n_b == 0) return 0;

    /* 공통 depth 범위에서 교차 검사 */
    int max_n = max_n_a < max_n_b ? max_n_a : max_n_b;

    for (int n = 1; n <= max_n; n++) {
        const uint8_t *ctx_wa = ctx_a->window + (ctx_a->win_len - n);
        const uint8_t *ctx_wb = ctx_b->window + (ctx_b->win_len - n);

        /* 각 Lane에서 모든 바이트의 빈도 수집 */
        for (int nb = 0; nb < 256; nb++) {
            uint32_t h_a = lane_mix(fnv(ctx_wa, n), lane_a);
            BtcCell *ca = cell_find(c, ctx_wa, n, (uint8_t)nb, h_a, 0);
            if (!ca || ca->G < 2) continue;

            uint32_t h_b = lane_mix(fnv(ctx_wb, n), lane_b);
            BtcCell *cb = cell_find(c, ctx_wb, n, (uint8_t)nb, h_b, 0);
            if (!cb || cb->G < 2) continue;

            /* 양쪽 다 활성 → 부스트 */
            uint32_t fa = g_to_freq(ca->G);
            uint32_t fb = g_to_freq(cb->G);
            ca->G = freq_to_g(fa + boost_factor);
            cb->G = freq_to_g(fb + boost_factor);
            boosted++;
        }
    }

    return boosted;
}

uint32_t btc_cross_boost_candle(BtCanvas *c,
                                uint8_t lane_a, BtcLaneCtx *ctx_a,
                                uint8_t lane_b, BtcLaneCtx *ctx_b,
                                uint32_t boost_factor,
                                uint32_t threshold)
{
    if (!c || !ctx_a || !ctx_b) return 0;
    if (boost_factor == 0) boost_factor = 1;

    uint32_t boosted = 0;
    int max_n_a = ctx_a->win_len < MAX_DEPTH ? ctx_a->win_len : MAX_DEPTH;
    int max_n_b = ctx_b->win_len < MAX_DEPTH ? ctx_b->win_len : MAX_DEPTH;
    if (max_n_a == 0 || max_n_b == 0) return 0;

    /*
     * 캔들 단위 교차: "방향 일치" 기반
     *
     * v6f 바이트에서 방향:
     *   > 128 = 상승, < 128 = 하락, == 128 = 무변동
     *
     * 양쪽 Lane의 최근 윈도우에서 "방향 시그니처"를 비교:
     *   최근 N 바이트의 평균 방향이 일치하면 부스트
     */
    int check_len = max_n_a < max_n_b ? max_n_a : max_n_b;
    if (check_len > 8) check_len = 8;  /* 최근 8바이트 (≈ 캔들 1~2개) */

    /* 각 Lane의 방향 점수 계산 */
    int dir_a = 0, dir_b = 0;
    uint32_t energy_a = 0, energy_b = 0;

    for (int i = 0; i < check_len; i++) {
        uint8_t ba = ctx_a->window[ctx_a->win_len - check_len + i];
        uint8_t bb = ctx_b->window[ctx_b->win_len - check_len + i];
        dir_a += (ba > 128) ? 1 : (ba < 128) ? -1 : 0;
        dir_b += (bb > 128) ? 1 : (bb < 128) ? -1 : 0;
        energy_a += (uint32_t)ba;
        energy_b += (uint32_t)bb;
    }

    /* 방향 일치 확인: 양쪽 모두 상승 or 양쪽 모두 하락 */
    int direction_match = (dir_a > 0 && dir_b > 0) || (dir_a < 0 && dir_b < 0);

    /* 에너지 임계값 확인 */
    uint32_t total_energy = energy_a + energy_b;
    if (!direction_match || total_energy < threshold) return 0;

    /* 방향이 일치하면: 양쪽 Lane의 최근 depth에서 모든 매칭 셀 부스트 */
    int max_d = check_len < MAX_DEPTH ? check_len : MAX_DEPTH;
    for (int n = 1; n <= max_d; n++) {
        if (n > ctx_a->win_len || n > ctx_b->win_len) break;

        const uint8_t *wa = ctx_a->window + (ctx_a->win_len - n);
        const uint8_t *wb = ctx_b->window + (ctx_b->win_len - n);

        /* Lane A의 활성 셀 부스트 */
        for (int nb = 0; nb < 256; nb++) {
            uint32_t h_a = lane_mix(fnv(wa, n), lane_a);
            BtcCell *ca = cell_find(c, wa, n, (uint8_t)nb, h_a, 0);
            if (ca && ca->G > 0) {
                uint32_t freq = g_to_freq(ca->G) + boost_factor;
                ca->G = freq_to_g(freq);
                boosted++;
            }
        }
        /* Lane B의 활성 셀 부스트 */
        for (int nb = 0; nb < 256; nb++) {
            uint32_t h_b = lane_mix(fnv(wb, n), lane_b);
            BtcCell *cb = cell_find(c, wb, n, (uint8_t)nb, h_b, 0);
            if (cb && cb->G > 0) {
                uint32_t freq = g_to_freq(cb->G) + boost_factor;
                cb->G = freq_to_g(freq);
                boosted++;
            }
        }
    }

    return boosted;
}

/* ══ Phase D: 예측 증발 (시뮬레이션) ════════════════ */

uint32_t btc_simulate_evaporate(const BtCanvas *src, BtCanvas *dst,
                                uint32_t rounds) {
    if (!src || !dst) return 0;

    /* src → dst 복사 */
    memcpy(dst, src, sizeof(BtCanvas));

    /* dst 위에서 증발 라운드 실행 */
    for (uint32_t r = 0; r < rounds; r++) {
        btc_evaporate(dst);
    }

    return dst->used;
}
