/*
 * sjds.h — Phase D3: SJ DataSet 범용 입력 포맷
 *
 * 어떤 도메인이든 BT 엔진에 넣기 위한 공식 입력 포맷.
 * 헤더 16바이트 + 바이트 스트림.
 *
 * 도메인: TEXT(0), FINANCE(1), CODE(2), CHAT(3)
 * 인코딩: RAW(0), V1(1), V6F(2)
 */
#ifndef SJDS_H
#define SJDS_H

#include <stdint.h>
#include <stdio.h>

#define SJDS_MAGIC   0x534A4453u  /* 'SJDS' */
#define SJDS_VERSION 1

/* 도메인 */
#define SJDS_DOMAIN_TEXT    0
#define SJDS_DOMAIN_FINANCE 1
#define SJDS_DOMAIN_CODE    2
#define SJDS_DOMAIN_CHAT    3

/* 인코딩 */
#define SJDS_ENC_RAW  0
#define SJDS_ENC_V1   1   /* 4B/캔들 (간단) */
#define SJDS_ENC_V6F  2   /* 7B/캔들 ±5% 고정 */

/* 타임프레임 */
#define SJDS_TF_NA    0x00
#define SJDS_TF_1M    0xF2
#define SJDS_TF_1H    0xF4
#define SJDS_TF_1D    0xF6

/* 결과 코드 */
#define SJDS_OK         0
#define SJDS_ERR_MAGIC -1
#define SJDS_ERR_IO    -2
#define SJDS_ERR_ALLOC -3

/* ── 헤더 (16B) ── */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;       /* SJDS_MAGIC */
    uint8_t  version;     /* SJDS_VERSION */
    uint8_t  domain;      /* SJDS_DOMAIN_* */
    uint8_t  encoding;    /* SJDS_ENC_* */
    uint8_t  timeframe;   /* SJDS_TF_* */
    uint32_t n_records;   /* 레코드 수 */
    uint32_t reserved;    /* 0 */
} SjdsHeader;  /* 16 bytes */
#pragma pack(pop)

/* ── API ── */

/*
 * sjds_write_header: .sjds 파일에 헤더 쓰기
 */
int sjds_write_header(FILE *f, uint8_t domain, uint8_t enc,
                      uint8_t timeframe, uint32_t n_records);

/*
 * sjds_append: .sjds 파일에 바이트 데이터 추가
 */
int sjds_append(FILE *f, const uint8_t *data, size_t len);

/*
 * sjds_info: .sjds 파일 헤더만 읽기
 */
int sjds_info(const char *path, SjdsHeader *out);

/*
 * sjds_create_from_text: 텍스트 파일 → .sjds 변환
 *
 * domain=SJDS_DOMAIN_TEXT, encoding=SJDS_ENC_RAW
 * 텍스트 바이트를 그대로 기록
 */
int sjds_create_from_text(const char *input_path, const char *output_path);

/*
 * sjds_load_and_train: .sjds 파일을 BtCanvas에 학습
 *
 * DK-EVAP: 내부에서 512바이트마다 btc_evaporate() 호출
 * 반환: 학습된 바이트 수 또는 에러 코드
 */
struct BtCanvas;  /* 전방 선언 */
int sjds_load_and_train(struct BtCanvas *canvas, const char *path);

#endif /* SJDS_H */
