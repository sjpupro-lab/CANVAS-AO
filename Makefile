CC     ?= gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2 -Iinclude
LDFLAGS = -lpthread

# Sanitizer flags (make sanitize)
SFLAGS   = -Wall -Wextra -std=c11 -g -fsanitize=address,undefined -Iinclude
SLDFLAGS = -lpthread -fsanitize=address,undefined

# ═══════════════════════════════════════════════════════════════════
# A/O Core Source Groups — AI이자 OS. 하나의 캔버스가 전부.
# ═══════════════════════════════════════════════════════════════════

# A/O 코어: 캔버스 엔진 기반
CORE_SRC = core/engine.c \
           core/scan_ringmh.c core/active_set.c core/canvasfs.c core/canvasfs_bpage.c \
           core/util/scheduler.c core/control_region.c core/engine_time.c core/gate_ops.c \
           core/vm/canvasos_opcodes.c core/cvp_io.c core/engine_ctx.c core/util/lane_exec.c \
           core/bpage_table.c core/inject.c core/wh_io.c core/canvas_lane.c \
           core/canvas_merge.c core/canvas_multiverse.c core/canvas_branch.c \
           core/canvas_bh_compress.c core/util/workers.c core/render/canvas_gpu_stub.c

# A/O 패턴 엔진 (6계층 + Constellation)
BT_SRC = core/bt_canvas.c core/bt_pattern.c core/bt_stream.c \
         core/bt_delta.c core/bt_live.c core/bt_canvas_render.c \
         core/bt_conv_ctrl.c core/sjds.c core/dataset.c core/ppl.c

# A/O 감정 엔진 (7D 벡터 + 파형 + 관계)
EMO_SRC = core/ai_stage.c
EMO_AUDIO_SRC = core/emo_audio.c
EMO_MELODY_SRC = core/sj_emotion_melody.c
RELATIONSHIP_SRC = core/sj_relationship.c
MEMORY_SRC = core/sj_memory_canvas.c
AI_CHAR_SRC = core/sj_ai_char.c
UTF8_SRC = core/sj_utf8.c

# A/O 압축 엔진
V3F_SRC = core/sj_spatial_v3f.c

# 시스템 계층
SYS_SRC = core/sys/proc.c core/sys/signal.c core/util/mprotect.c core/sys/pipe.c \
          core/sys/syscall.c core/detmode.c core/sys/fd.c core/sys/fd_canvas_bridge.c \
          core/util/path.c core/util/path_virtual.c

# VM 계층
VM_SRC = core/vm/vm.c core/vm/pixelcode.c core/sys/syscall.c core/vm/vm_runtime_bridge.c

# 커널 서브셋
P8_KERN = core/sys/proc.c core/sys/signal.c core/util/mprotect.c core/sys/pipe.c \
          core/sys/syscall.c core/detmode.c core/util/timewarp.c
P9_VM   = core/vm/vm.c core/vm/pixelcode.c
P10_SRC = core/sys/fd.c core/util/path.c core/sys/user.c core/util/utils.c \
          apps/terminal/shell.c core/sys/fd_canvas_bridge.c core/util/path_virtual.c \
          core/sys/syscall_bindings.c core/vm/vm_runtime_bridge.c \
          core/render/pixel_loader.c core/util/timeline.c core/util/livedemo.c
KERN_SRC = core/sys/proc.c core/sys/signal.c core/sys/pipe.c core/sys/fd.c \
           core/sys/fd_canvas_bridge.c core/util/path.c core/util/path_virtual.c

# GUI
GUI_SRC = core/render/canvasos_gui.c

# Tervas
TERVAS_SRC = apps/terminal/tervas_core.c apps/terminal/tervas_bridge.c \
             apps/terminal/tervas_cli.c apps/terminal/tervas_projection.c \
             apps/terminal/tervas_render_ascii.c apps/terminal/tervas_frame.c \
             apps/terminal/tervas_render_fb.c apps/terminal/tervas_render_sdl2_stub.c

# CanvasCraft
CC_SRC        = apps/craft/canvascraft.c
CC_PLAY_SRC   = apps/craft/canvascraft_play.c
CC_PROC_SRC   = apps/craft/canvascraft_proc.c
CC_RENDER_SRC = apps/craft/canvascraft_render.c
CC_GPU_SRC    = apps/craft/canvascraft_gpu.c
CC_TERR_SRC   = apps/craft/canvascraft_territory.c
CC_NET_SRC    = apps/craft/canvascraft_net.c
CC_PERSIST_SRC = apps/craft/canvascraft_persist.c
CC_INTERACT_SRC = apps/craft/canvascraft_interact.c
CC_MULTIVIEW_SRC = apps/craft/canvascraft_multiview.c
CC_AI_SRC     = apps/craft/canvascraft_ai.c

# AI 파이프라인 의존성
AI_DEPS = core/sj_ai_pipeline.c $(CC_AI_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) \
          $(CORE_SRC) apps/terminal/sjptl_parser.c $(KERN_SRC) core/sj_pattern.c \
          core/sj_stream_bridge.c core/sj_autoexec.c \
          core/sj_stream_loader.c core/net/sj_stream_packet.c
SKY_RENDER_SRC = core/sj_sky_render.c core/render/canvasos_gui.c
TOK_SRC = core/sj_tokenizer.c

# ─── Engine binary ───
ENGINE_SRC = apps/terminal/canvasos_cli.c $(CORE_SRC) apps/terminal/sjptl_parser.c
ENGINE_BIN = canvasos

.PHONY: all run test clean cli ao

all: $(ENGINE_BIN)

ao: $(ENGINE_BIN)

$(ENGINE_BIN): $(ENGINE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(ENGINE_BIN)
	./$(ENGINE_BIN)

# ═══════════════════════════════════════════════════════════════════
# Core Tests — A/O 코어 검증
# ═══════════════════════════════════════════════════════════════════

tests/test_scan: tests/test_scan.c core/scan_ringmh.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_gate: tests/test_gate.c core/active_set.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvasfs: tests/test_canvasfs.c core/canvasfs.c core/canvasfs_bpage.c core/active_set.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_scheduler: tests/test_scheduler.c core/util/scheduler.c core/active_set.c core/canvasfs.c core/canvasfs_bpage.c core/engine_time.c core/gate_ops.c core/vm/canvasos_opcodes.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_cvp: tests/test_cvp.c core/cvp_io.c core/engine_ctx.c core/util/lane_exec.c core/bpage_table.c core/inject.c core/wh_io.c core/canvas_lane.c core/canvas_merge.c core/canvas_multiverse.c core/canvas_branch.c core/canvas_bh_compress.c core/util/workers.c core/render/canvas_gpu_stub.c apps/terminal/sjptl_parser.c core/util/scheduler.c core/engine_time.c core/gate_ops.c core/vm/canvasos_opcodes.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: tests/test_scan tests/test_gate tests/test_canvasfs tests/test_scheduler tests/test_cvp
	@echo "=== A/O Core Tests ==="
	@./tests/test_scan
	@./tests/test_gate
	@./tests/test_canvasfs
	@./tests/test_scheduler
	@./tests/test_cvp
	@echo "=== CORE: ALL PASS ==="

# ═══════════════════════════════════════════════════════════════════
# Phase Tests
# ═══════════════════════════════════════════════════════════════════

tests/test_phase6: tests/test_phase6.c $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

PHASE8_SRC = $(SYS_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c

tests/test_phase8: tests/test_phase8.c $(PHASE8_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_phase9: tests/test_phase9.c $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c core/sys/proc.c core/sys/signal.c core/sys/pipe.c core/sys/fd.c core/sys/fd_canvas_bridge.c core/util/path.c core/util/path_virtual.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_phase10: tests/test_phase10.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_bridge: tests/test_bridge.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ═══════════════════════════════════════════════════════════════════
# Patch Tests
# ═══════════════════════════════════════════════════════════════════

tests/test_patchB: tests/test_patchB.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_patchC: tests/test_patchC.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_patchD: tests/test_patchD.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_patchE: tests/test_patchE.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_patchF: tests/test_patchF.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_patchG: tests/test_patchG.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_patchH: tests/test_patchH.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ═══════════════════════════════════════════════════════════════════
# A/O Pattern + Stream + Compression Tests
# ═══════════════════════════════════════════════════════════════════

BT_CORE = core/bt_canvas.c core/bt_pattern.c
BT_STREAM = core/bt_stream.c core/bt_canvas_render.c $(BT_CORE)
BT_DELTA = core/bt_delta.c $(BT_STREAM)
BT_LIVE = core/bt_live.c $(BT_DELTA)

tests/test_phase_c_compress: tests/test_phase_c_compress.c $(BT_CORE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_bt_stream: tests/test_bt_stream.c $(BT_STREAM)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_bt_delta: tests/test_bt_delta.c $(BT_DELTA)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_bt_live: tests/test_bt_live.c $(BT_LIVE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_phase_d_compress: tests/test_phase_d_compress.c $(BT_LIVE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_sjds: tests/test_sjds.c core/sjds.c $(BT_CORE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_canvas_render: tests/test_canvas_render.c core/bt_canvas_render.c $(BT_CORE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_conv_ctrl: tests/test_conv_ctrl.c core/bt_conv_ctrl.c $(BT_CORE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_sjqb: tests/test_sjqb.c $(BT_LIVE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_multilane: tests/test_multilane.c $(BT_LIVE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_delta_merge: tests/test_delta_merge.c $(BT_LIVE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# ═══════════════════════════════════════════════════════════════════
# A/O Emotion + AI Stage Tests
# ═══════════════════════════════════════════════════════════════════

tests/test_ai_stage: tests/test_ai_stage.c $(EMO_SRC) $(BT_LIVE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/test_pipeline: tests/test_pipeline.c $(EMO_SRC) core/sjds.c $(BT_LIVE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# ═══════════════════════════════════════════════════════════════════
# A/O AI Pipeline (Constellation + Tokenizer)
# ═══════════════════════════════════════════════════════════════════

tests/test_ai_pipeline: tests/test_ai_pipeline.c $(AI_DEPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -Wno-stringop-truncation

tests/test_tokenizer: tests/test_tokenizer.c $(TOK_SRC) $(AI_DEPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -Wno-stringop-truncation

tests/test_constellation_bench: tests/test_constellation_bench.c $(SKY_RENDER_SRC) $(AI_DEPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -Wno-stringop-truncation

tests/test_phase_c: tests/test_phase_c.c $(AI_DEPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -Wno-stringop-truncation

tests/test_phase_df: tests/test_phase_df.c $(AI_DEPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -Wno-stringop-truncation

# ═══════════════════════════════════════════════════════════════════
# A/O Spatial Compression Tests
# ═══════════════════════════════════════════════════════════════════

tests/test_v3f_stress: tests/test_v3f_stress.c $(V3F_SRC)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# ═══════════════════════════════════════════════════════════════════
# Cell I/O Tests
# ═══════════════════════════════════════════════════════════════════

tests/test_cell_io: tests/test_cell_io.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_cell_io_stress: tests/test_cell_io_stress.c $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ═══════════════════════════════════════════════════════════════════
# GUI + Render Tests
# ═══════════════════════════════════════════════════════════════════

tests/test_gui: tests/test_gui.c $(GUI_SRC)
	$(CC) $(CFLAGS) -o $@ $^

tests/test_gui_bridge: tests/test_gui_bridge.c core/render/gui_engine_bridge.c $(GUI_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_phase8f_compositor: tests/test_phase8f_compositor.c core/render/compositor.c $(GUI_SRC)
	$(CC) $(CFLAGS) -o $@ $^

# ═══════════════════════════════════════════════════════════════════
# VM / Opcode Tests
# ═══════════════════════════════════════════════════════════════════

tests/test_engcode: tests/test_engcode.c core/vm/engcode.c $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_hgcode: tests/test_hgcode.c core/vm/hgcode.c $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ═══════════════════════════════════════════════════════════════════
# Phase 8f Tests
# ═══════════════════════════════════════════════════════════════════

tests/test_phase8f_renderer: tests/test_phase8f_renderer.c $(TERVAS_SRC) $(GUI_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_phase8f_event_bus: tests/test_phase8f_event_bus.c core/sys/event_bus.c core/render/mouse.c $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_phase8f_frame_loop: tests/test_phase8f_frame_loop.c core/render/frame_loop.c core/sys/event_bus.c core/render/mouse.c $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_phase8f_sched: tests/test_phase8f_sched.c core/util/sched_v2.c core/sys/proc.c core/sys/signal.c core/engine_ctx.c core/cvp_io.c core/gate_ops.c core/wh_io.c core/engine_time.c core/vm/canvasos_opcodes.c core/util/scheduler.c core/active_set.c core/canvas_bh_compress.c core/canvas_lane.c core/canvas_merge.c core/canvas_multiverse.c core/canvas_branch.c core/util/workers.c core/render/canvas_gpu_stub.c apps/terminal/sjptl_parser.c core/bpage_table.c core/inject.c core/util/lane_exec.c core/canvasfs.c core/canvasfs_bpage.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_phase8f_uri: tests/test_phase8f_uri.c core/util/uri.c
	$(CC) $(CFLAGS) -o $@ $^

# ═══════════════════════════════════════════════════════════════════
# Stream / Pattern / Autoexec Tests
# ═══════════════════════════════════════════════════════════════════

tests/test_sj_stream_roundtrip: tests/test_sj_stream_roundtrip.c core/net/sj_stream_packet.c
	$(CC) $(CFLAGS) -o $@ $^

tests/test_pattern: tests/test_pattern.c core/sj_pattern.c core/sj_stream_loader.c
	$(CC) $(CFLAGS) -o $@ $^ -Wno-stringop-truncation

tests/test_autoexec: tests/test_autoexec.c core/sj_autoexec.c core/sj_pattern.c core/sj_stream_loader.c
	$(CC) $(CFLAGS) -o $@ $^ -Wno-stringop-truncation

tests/test_stream_bridge: tests/test_stream_bridge.c core/sj_stream_bridge.c core/sj_autoexec.c core/sj_pattern.c core/sj_stream_loader.c core/net/sj_stream_packet.c
	$(CC) $(CFLAGS) -o $@ $^ -Wno-stringop-truncation

# ═══════════════════════════════════════════════════════════════════
# Benchmark
# ═══════════════════════════════════════════════════════════════════

tests/test_benchmark: tests/test_benchmark.c core/render/gui_engine_bridge.c $(GUI_SRC) $(P10_SRC) $(P8_KERN) $(P9_VM) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# A/O 벤치마크: Phase B (진짜 패턴 엔진)
benchmark/bench_phaseB: benchmark/bench_phaseB.c $(BT_LIVE) core/dataset.c core/ppl.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Scaling benchmark
tests/bench_scaling: tests/bench_scaling.c $(BT_STREAM) core/ppl.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/bench_phase_d: tests/bench_phase_d.c $(BT_LIVE) core/ppl.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/bench_multilane: tests/bench_multilane.c $(BT_LIVE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

tests/bench_v6f: tests/bench_v6f.c $(BT_LIVE)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# enwik8 벤치마크 (또는 자기 소스코드 벤치마크)
benchmark/bench_enwik8: benchmark/bench_enwik8.c $(BT_STREAM) core/ppl.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

bench_enwik8: benchmark/bench_enwik8
	@./benchmark/bench_enwik8

bench: benchmark/bench_phaseB tests/bench_scaling
	@echo "=== A/O Benchmark ==="
	@./benchmark/bench_phaseB
	@./tests/bench_scaling

# ═══════════════════════════════════════════════════════════════════
# Apps — A/O 코어가 각 모드로 동작
# ═══════════════════════════════════════════════════════════════════

# Tervas 터미널
TERVAS_BIN = tervas

$(TERVAS_BIN): apps/terminal/tervas_main.c $(TERVAS_SRC) $(GUI_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_tervas: tests/test_tervas.c $(TERVAS_SRC) $(GUI_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

terminal: $(TERVAS_BIN)

# CanvasCraft
CC_ALL_SRC = $(CC_SRC) $(CC_PLAY_SRC) $(CC_PROC_SRC) $(CC_TERR_SRC) $(CC_NET_SRC) $(CC_PERSIST_SRC) $(CC_RENDER_SRC)

tests/test_canvascraft: tests/test_canvascraft.c $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_play: tests/test_canvascraft_play.c $(CC_PLAY_SRC) $(CC_RENDER_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(GUI_SRC) $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_proc: tests/test_canvascraft_proc.c $(CC_PROC_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_render: tests/test_canvascraft_render.c $(CC_RENDER_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(GUI_SRC) $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_render_adv: tests/test_canvascraft_render_adv.c $(CC_RENDER_SRC) $(CC_GPU_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(GUI_SRC) $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -ldl

tests/test_canvascraft_compose: tests/test_canvascraft_compose.c $(CC_RENDER_SRC) $(CC_PLAY_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(GUI_SRC) $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_territory: tests/test_canvascraft_territory.c $(CC_TERR_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_net: tests/test_canvascraft_net.c $(CC_NET_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c core/net/sj_stream_packet.c $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_persist: tests/test_canvascraft_persist.c $(CC_PERSIST_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_interact: tests/test_canvascraft_interact.c $(CC_INTERACT_SRC) $(CC_RENDER_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(GUI_SRC) $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_multiview: tests/test_canvascraft_multiview.c $(CC_MULTIVIEW_SRC) $(CC_RENDER_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(GUI_SRC) $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_canvascraft_ai: tests/test_canvascraft_ai.c $(CC_AI_SRC) $(CC_SRC) $(V3F_SRC) $(VM_SRC) $(CORE_SRC) apps/terminal/sjptl_parser.c $(KERN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

craft: tests/test_canvascraft
	@echo "=== CanvasCraft ==="
	@./tests/test_canvascraft

# BT Chat — A/O가 대화 모드로 동작
BT_CHAT_BIN = bt_chat

$(BT_CHAT_BIN): core/bt_chat.c $(TOK_SRC) $(SKY_RENDER_SRC) $(AI_DEPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -Wno-stringop-truncation

chat: $(BT_CHAT_BIN)

# SJ Chat — 한국어 A/O 대화 (omnara 엔진)
SJ_CHAT_BIN = sj_chat
OMNARA_DEPS = $(AI_CHAR_SRC) $(EMO_MELODY_SRC) $(MEMORY_SRC) $(RELATIONSHIP_SRC) $(UTF8_SRC)

$(SJ_CHAT_BIN): core/sj_chat.c $(OMNARA_DEPS) $(BT_LIVE) core/bt_conv_ctrl.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

sj_chat: $(SJ_CHAT_BIN)

# ═══════════════════════════════════════════════════════════════════
# A/O Emotion Tests (emo_audio, emotion_melody, relationship)
# ═══════════════════════════════════════════════════════════════════

tests/test_emo_audio: tests/test_emo_audio.c $(EMO_AUDIO_SRC)
	$(CC) $(CFLAGS) -o $@ $^

emo_audio_test: tests/test_emo_audio
	@echo "=== A/O Emotion Audio ==="
	@./tests/test_emo_audio

.PHONY: emo_audio_test sj_chat triple_chain_test cascade_test

# ═══════════════════════════════════════════════════════════════════
# Triple Chain (텍스트 + 오디오 + 감정)
# ═══════════════════════════════════════════════════════════════════

TRIPLE_CHAIN_SRC = core/triple_chain.c

tests/test_triple_chain: tests/test_triple_chain.c $(TRIPLE_CHAIN_SRC) $(BT_CORE)
	$(CC) $(CFLAGS) -o $@ $^

triple_chain_test: tests/test_triple_chain
	@echo "=== Triple Chain Test ==="
	@./tests/test_triple_chain

# ═══════════════════════════════════════════════════════════════════
# Cascade Layer (정밀도 캐스케이드 — float 없이 float급 정밀도)
# ═══════════════════════════════════════════════════════════════════

CASCADE_SRC = core/cascade_layer.c

tests/test_cascade: tests/test_cascade.c $(CASCADE_SRC) $(BT_CORE)
	$(CC) $(CFLAGS) -o $@ $^

cascade_test: tests/test_cascade
	@echo "=== Cascade Layer Test ==="
	@./tests/test_cascade

# ═══════════════════════════════════════════════════════════════════
# Full Test Suite
# ═══════════════════════════════════════════════════════════════════

test_all: test tests/test_phase6 tests/test_phase8 tests/test_phase9 tests/test_phase10 \
          tests/test_bridge tests/test_patchB tests/test_patchC tests/test_patchD \
          tests/test_patchE tests/test_patchF tests/test_patchG tests/test_patchH \
          tests/test_sj_stream_roundtrip tests/test_gui \
          tests/test_phase_c_compress tests/test_bt_stream tests/test_bt_delta \
          tests/test_bt_live tests/test_sjds tests/test_canvas_render tests/test_conv_ctrl \
          tests/test_ai_stage tests/test_pipeline \
          tests/test_ai_pipeline tests/test_tokenizer \
          tests/test_tervas tests/test_canvascraft
	@echo "=== A/O Full Test Suite ==="
	@./tests/test_phase6
	@./tests/test_phase8
	@./tests/test_phase9
	@./tests/test_phase10
	@./tests/test_bridge
	@./tests/test_patchB
	@./tests/test_patchC
	@./tests/test_patchD
	@./tests/test_patchE
	@./tests/test_patchF
	@./tests/test_patchG
	@./tests/test_patchH
	@./tests/test_sj_stream_roundtrip
	@./tests/test_gui
	@echo "=== A/O Pattern Engine ==="
	@./tests/test_phase_c_compress
	@./tests/test_bt_stream
	@./tests/test_bt_delta
	@./tests/test_bt_live
	@./tests/test_sjds
	@./tests/test_canvas_render
	@./tests/test_conv_ctrl
	@echo "=== A/O Emotion Engine ==="
	@./tests/test_ai_stage
	@./tests/test_pipeline
	@echo "=== A/O Constellation ==="
	@./tests/test_ai_pipeline
	@./tests/test_tokenizer
	@echo "=== A/O Terminal ==="
	@./tests/test_tervas
	@echo "=== A/O Craft ==="
	@./tests/test_canvascraft
	@echo ""
	@echo "╔══════════════════════════════════════╗"
	@echo "║     A/O = AI/OS = 하나. ALL PASS     ║"
	@echo "╚══════════════════════════════════════╝"

.PHONY: test_all terminal craft chat bench

# ═══════════════════════════════════════════════════════════════════
# CLI
# ═══════════════════════════════════════════════════════════════════

CLI_BIN = canvasos_cli

$(CLI_BIN): apps/terminal/canvasos_cli.c $(CORE_SRC) apps/terminal/sjptl_parser.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

cli: $(CLI_BIN)

.PHONY: cli

# ═══════════════════════════════════════════════════════════════════
# Clean
# ═══════════════════════════════════════════════════════════════════

clean:
	rm -f $(ENGINE_BIN) $(CLI_BIN) $(TERVAS_BIN) $(BT_CHAT_BIN)
	rm -f tests/test_scan tests/test_gate tests/test_canvasfs tests/test_scheduler tests/test_cvp
	rm -f tests/test_phase6 tests/test_phase8 tests/test_phase9 tests/test_phase10
	rm -f tests/test_bridge tests/test_benchmark
	rm -f tests/test_patchB tests/test_patchC tests/test_patchD tests/test_patchE
	rm -f tests/test_patchF tests/test_patchG tests/test_patchH
	rm -f tests/test_cell_io tests/test_cell_io_stress
	rm -f tests/test_engcode tests/test_hgcode
	rm -f tests/test_phase8f_renderer tests/test_phase8f_event_bus tests/test_phase8f_frame_loop
	rm -f tests/test_phase8f_sched tests/test_phase8f_uri tests/test_phase8f_compositor
	rm -f tests/test_gui tests/test_gui_bridge
	rm -f tests/test_pattern tests/test_autoexec tests/test_stream_bridge
	rm -f tests/test_sj_stream_roundtrip
	rm -f tests/test_phase_c_compress tests/test_bt_stream tests/test_bt_delta
	rm -f tests/test_bt_live tests/test_phase_d_compress tests/test_sjds
	rm -f tests/test_canvas_render tests/test_conv_ctrl tests/test_sjqb
	rm -f tests/test_multilane tests/test_delta_merge
	rm -f tests/test_ai_stage tests/test_pipeline
	rm -f tests/test_ai_pipeline tests/test_tokenizer tests/test_constellation_bench
	rm -f tests/test_phase_c tests/test_phase_df
	rm -f tests/test_v3f_stress
	rm -f tests/test_tervas tests/test_canvascraft tests/test_canvascraft_play
	rm -f tests/test_canvascraft_proc tests/test_canvascraft_render tests/test_canvascraft_render_adv
	rm -f tests/test_canvascraft_compose tests/test_canvascraft_territory
	rm -f tests/test_canvascraft_net tests/test_canvascraft_persist
	rm -f tests/test_canvascraft_interact tests/test_canvascraft_multiview tests/test_canvascraft_ai
	rm -f tests/bench_phase_d tests/bench_scaling tests/bench_multilane tests/bench_v6f
	rm -f benchmark/bench_phaseB
	rm -f *.cvp tests/test_gui_output.bmp
	rm -f tests/test_emo_audio
	rm -f $(SJ_CHAT_BIN)
	rm -f tests/_test_heatmap.bmp tests/_test_heatmap256.bmp tests/_test_depthmap.bmp
	rm -rf tests/_test_frames tests/_test_brain.stream
