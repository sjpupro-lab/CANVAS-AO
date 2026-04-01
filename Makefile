CC     = gcc
CFLAGS = -O2 -Wall -Iinclude -Icore
LDFLAGS = -lm

CORE_SRCS = core/canvas.c core/gate.c core/scan.c core/pattern.c \
            core/constellation.c core/emotion.c core/emotion_detect.c \
            core/stream.c core/compress.c core/elo.c core/v6f.c \
            core/wh.c core/bh.c core/merge.c core/lane.c core/branch.c \
            core/multiverse.c core/scheduler.c core/fs.c core/chat.c

all: ao dating finance terminal craft train benchmark test

ao: $(CORE_SRCS)
	$(CC) $(CFLAGS) -shared -fPIC -o libao.so $(CORE_SRCS) $(LDFLAGS)

dating: $(CORE_SRCS) apps/dating/dating_engine.c apps/dating/persona.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/dating $(CORE_SRCS) apps/dating/dating_engine.c apps/dating/persona.c -DDATING_MAIN $(LDFLAGS)

finance: $(CORE_SRCS) apps/finance/finance_bench.c apps/finance/multiverse_bench.c apps/finance/anomaly.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/finance $(CORE_SRCS) apps/finance/finance_bench.c apps/finance/multiverse_bench.c apps/finance/anomaly.c -DFINANCE_MAIN $(LDFLAGS)

terminal: $(CORE_SRCS) apps/terminal/terminal_main.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/terminal $(CORE_SRCS) apps/terminal/terminal_main.c $(LDFLAGS)

craft: $(CORE_SRCS) apps/craft/craft_main.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/craft $(CORE_SRCS) apps/craft/craft_main.c $(LDFLAGS)

train: $(CORE_SRCS) apps/train/train_main.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/train $(CORE_SRCS) apps/train/train_main.c $(LDFLAGS)

benchmark: $(CORE_SRCS) benchmark/bench_phaseB.c benchmark/bench_phase_d.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/bench_phaseB $(CORE_SRCS) benchmark/bench_phaseB.c $(LDFLAGS)
	$(CC) $(CFLAGS) -o bin/bench_phase_d $(CORE_SRCS) benchmark/bench_phase_d.c $(LDFLAGS)

bench_enwik8: $(CORE_SRCS) benchmark/bench_enwik8.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/bench_enwik8 $(CORE_SRCS) benchmark/bench_enwik8.c $(LDFLAGS)

test: $(CORE_SRCS) tests/test_core.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/test_core $(CORE_SRCS) tests/test_core.c $(LDFLAGS)
	./bin/test_core

clean:
	rm -f bin/* libao.so

.PHONY: all ao dating finance terminal craft train benchmark bench_enwik8 test clean
