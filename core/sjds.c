/*
 * sjds.c — Phase D3: SJ DataSet 범용 입력 포맷
 *
 * 어떤 도메인이든 [헤더 16B] + [바이트 스트림] 으로 통일.
 * BtCanvas에 학습 시 DK-EVAP (512바이트마다 증발) 자동 적용.
 */
#include "sjds.h"
#include "bt_canvas.h"
#include <string.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════ */

int sjds_write_header(FILE *f, uint8_t domain, uint8_t enc,
                      uint8_t timeframe, uint32_t n_records) {
    if (!f) return SJDS_ERR_IO;

    SjdsHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = SJDS_MAGIC;
    hdr.version = SJDS_VERSION;
    hdr.domain = domain;
    hdr.encoding = enc;
    hdr.timeframe = timeframe;
    hdr.n_records = n_records;

    if (fwrite(&hdr, sizeof(hdr), 1, f) != 1) return SJDS_ERR_IO;
    return SJDS_OK;
}

/* ══════════════════════════════════════════════ */

int sjds_append(FILE *f, const uint8_t *data, size_t len) {
    if (!f || !data) return SJDS_ERR_IO;
    if (fwrite(data, 1, len, f) != len) return SJDS_ERR_IO;
    return SJDS_OK;
}

/* ══════════════════════════════════════════════ */

int sjds_info(const char *path, SjdsHeader *out) {
    if (!path || !out) return SJDS_ERR_IO;

    FILE *f = fopen(path, "rb");
    if (!f) return SJDS_ERR_IO;

    if (fread(out, sizeof(*out), 1, f) != 1) {
        fclose(f);
        return SJDS_ERR_IO;
    }
    fclose(f);

    if (out->magic != SJDS_MAGIC) return SJDS_ERR_MAGIC;
    return SJDS_OK;
}

/* ══════════════════════════════════════════════ */

int sjds_create_from_text(const char *input_path, const char *output_path) {
    if (!input_path || !output_path) return SJDS_ERR_IO;

    /* 입력 파일 크기 측정 */
    FILE *in = fopen(input_path, "rb");
    if (!in) return SJDS_ERR_IO;
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);

    /* 출력 파일 */
    FILE *out = fopen(output_path, "wb");
    if (!out) { fclose(in); return SJDS_ERR_IO; }

    /* 헤더 쓰기 */
    sjds_write_header(out, SJDS_DOMAIN_TEXT, SJDS_ENC_RAW,
                      SJDS_TF_NA, (uint32_t)size);

    /* 바이트 복사 */
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);

    fclose(in);
    fclose(out);
    return SJDS_OK;
}

/* ══════════════════════════════════════════════ */

int sjds_load_and_train(struct BtCanvas *canvas, const char *path) {
    if (!canvas || !path) return SJDS_ERR_IO;

    FILE *f = fopen(path, "rb");
    if (!f) return SJDS_ERR_IO;

    /* 헤더 읽기 */
    SjdsHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return SJDS_ERR_IO; }
    if (hdr.magic != SJDS_MAGIC) { fclose(f); return SJDS_ERR_MAGIC; }

    /* 바이트 스트림 학습 */
    uint8_t buf[4096];
    size_t n;
    uint32_t total = 0;
    uint32_t evap_counter = 0;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            btc_train(canvas, buf[i]);
            total++;
            evap_counter++;

            /* DK-EVAP: 512바이트마다 증발 */
            if (evap_counter >= 512) {
                btc_evaporate(canvas);
                evap_counter = 0;
            }
        }
    }

    fclose(f);
    return (int)total;
}
