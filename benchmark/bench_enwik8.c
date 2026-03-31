#include "canvasos.h"
#include "cell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define SYNTHETIC_SIZE (1024 * 1024) /* 1 MB synthetic if enwik8 not found */

static void bench_enwik8_run(const char *filename) {
    printf("=== enwik8 A/O Compression Benchmark ===\n");

    uint8_t *input  = NULL;
    size_t   input_size = 0;
    FILE *f = NULL;

    if (filename) f = fopen(filename, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        long fsz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fsz > 0) {
            input_size = (size_t)fsz;
            input = (uint8_t *)malloc(input_size);
            if (input) {
                size_t rd = fread(input, 1, input_size, f);
                input_size = rd;
            }
        }
        fclose(f);
        printf("Source         : %s (%zu bytes)\n", filename, input_size);
    } else {
        printf("enwik8 not found — using synthetic %d-byte data\n", SYNTHETIC_SIZE);
        input_size = SYNTHETIC_SIZE;
        input = (uint8_t *)malloc(input_size);
        if (input) {
            /* Natural-language-like synthetic: low entropy */
            for (size_t i = 0; i < input_size; i++)
                input[i] = (uint8_t)(' ' + (i * 7 + (i >> 3)) % 64);
        }
    }

    if (!input) { fprintf(stderr, "malloc failed\n"); return; }

    /* Init A/O canvas */
    canvas_init();

    /* Allocate output buffer */
    size_t   out_size = input_size + input_size / 4 + 16;
    uint8_t *output   = (uint8_t *)malloc(out_size);
    if (!output) { free(input); fprintf(stderr, "malloc failed\n"); return; }

    clock_t t0 = clock();
    int compressed_size = compress_predicted_delta(input, input_size, output, out_size);
    clock_t t1 = clock();

    if (compressed_size < 0) {
        printf("Compression failed\n");
    } else {
        double elapsed  = (double)(t1 - t0) / CLOCKS_PER_SEC;
        double ratio    = (double)input_size / compressed_size;
        double bpb      = (8.0 * compressed_size) / input_size;
        double speed_mb = (elapsed > 0) ? (input_size / 1e6) / elapsed : 0.0;

        printf("Original size  : %zu bytes\n", input_size);
        printf("Compressed size: %d bytes\n", compressed_size);
        printf("Ratio          : %.3fx\n", ratio);
        printf("BPB            : %.4f bits/byte\n", bpb);
        printf("Speed          : %.2f MB/s\n", speed_mb);
        printf("Elapsed        : %.3f s\n", elapsed);

        /* Verify round-trip */
        uint8_t *recovered = (uint8_t *)malloc(input_size + 16);
        if (recovered) {
            int rec_size = compress_decompress(output, (size_t)compressed_size,
                                               recovered, input_size + 16);
            int ok = (rec_size > 0) && ((size_t)rec_size == input_size) &&
                     (memcmp(input, recovered, input_size) == 0);
            printf("Round-trip     : %s\n", ok ? "OK" : "FAIL");
            free(recovered);
        }
    }
    printf("=== enwik8 DONE ===\n");
    free(input);
    free(output);
}

int main(int argc, char *argv[]) {
    const char *fname = (argc > 1) ? argv[1] : "data/enwik8";
    bench_enwik8_run(fname);
    return 0;
}
