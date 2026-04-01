# CANVAS-AO Merge Log

## Integration Summary

SJ-CANVAOS concepts merged into CANVAS-AO unified codebase.

---

## Commits

### 1. compress.c rewrite (95a7984)
- **What**: Replaced naive weighted-average XOR-delta with A/O pipeline executor
- **Why**: Original compressor had no learning capability. New pipeline uses 6-order FNV-1a n-gram prediction with dual output format (corrections vs XOR-delta)
- **Key decisions**:
  - FORMAT_CORRECTIONS (0x01) for high-accuracy prediction, FORMAT_XOR_DELTA (0x00) fallback
  - Corrections format: `[4B size][1B tag=0x01][4B count][index:delta pairs]`
  - Auto-selects format based on which produces smaller output

### 2. ELO cascade (e85bf71)
- **What**: 3-layer integer-only precision system (core/elo.c)
- **Why**: DK-2 forbids float, but multi-scale temporal patterns need fractional precision. ELO achieves this through cascading integer layers at different time scales
- **Key decisions**:
  - Layer 3 (100x speed, byte-pairs) -> Layer 2 (10x, quad-grams) -> Layer 1 (1x, hex-grams)
  - Trust propagation via integer EMA: `trust = (trust*3 + hit_rate + 2) >> 2`
  - Combined weight: `order * trust * confidence` preserves static order preference

### 3. DK-2 integer conversion (4053edb)
- **What**: Eliminated all 162 float/double instances across 17 files
- **Why**: DK-2 absolute rule: no floating point in any code path
- **Key decisions**:
  - EmotionVector: float -> uint8_t (0-255)
  - V6F: float[6] -> int32_t[6] (fixed-point x100)
  - Universe.probability: float -> uint16_t (0-65535)
  - compress_ratio: float -> uint32_t (x256 fixed-point)
  - Decay: `val = (val * 250 + 128) >> 8` replaces `val *= 0.98f`
  - Blend: `(a * (255-r) + b * r + 128) >> 8` replaces `a * (1-r) + b * r`
  - Distance: squared integer comparison replaces sqrtf

### 4. Integration features (current)
- **3-2**: N-gram language patterns added to pattern.c (6-layer: byte/morpheme/word/phrase/clause/sentence)
- **3-3**: `constellation_apply_emotion()` connects emotion dominant to node energy
- **3-4**: Dating engine uses branch/multiverse for 3-candidate response diversity
- **3-5**: 2-stage chat generation engine (word n-gram -> byte fallback)
- **3-7**: 85 new tests (ELO, constellation-emotion, lang pattern, chat, DK-2 compliance)

---

## Architecture Connection Diagram

```
                          +------------------+
                          |   Applications   |
                          +------------------+
                          |  dating_engine   |---- persona.c
                          |  finance_bench   |---- anomaly.c
                          |  multiverse_bench|
                          |  terminal_main   |
                          |  craft_main      |
                          +--------+---------+
                                   |
          +------------------------+------------------------+
          |                        |                        |
   +------v------+        +-------v-------+        +-------v-------+
   |   Emotion   |------->| Constellation |        |   Multiverse  |
   |  emotion.c  |  apply |constellation.c|        | multiverse.c  |
   | uint8_t x7  |emotion | uint8_t energy|        | uint16_t prob |
   +------+------+        +-------+-------+        +-------+-------+
          |                        |                        |
          |                +-------v-------+        +-------v-------+
          |                |    Pattern     |        |    Branch     |
          |                |  pattern.c     |        |   branch.c   |
          |                | spatial(6) +   |        | COW patches   |
          |                | lang_ngram(6)  |        +---------------+
          |                +---------------+
          |
   +------v------+
   |    Chat      |  2-stage: word n-gram -> byte fallback
   |   chat.c     |  uses pattern_lang engine
   +------+------+
          |
   +------v------+     +----------+     +---------+
   |  Compress   |---->|    WH    |---->|   BH    |
   | compress.c  |     |  wh.c    |     |  bh.c   |
   | A/O pipeline|     | records  |     | summary |
   +------+------+     +----------+     +---------+
          |
   +------v------+     +----------+
   |    ELO      |---->|  Stream  |
   |   elo.c     |     | stream.c |
   | 3-layer     |     | KF+delta |
   | cascade     |     +----------+
   +-------------+
          |
   +------v------+
   |   Canvas    |     4096x4096 cells, 8B each = 128MB
   |  canvas.c   |     ABGR(4) + energy(1) + state(1) + id(2)
   +------+------+
          |
   +------v------+     +----------+     +---------+
   |    Gate     |     |   Scan   |     |  Merge  |
   |   gate.c   |     |  scan.c  |     | merge.c |
   +-------------+     +----------+     +---------+
          |
   +------v------+     +----------+
   |    Lane     |     | Scheduler|
   |   lane.c   |     |scheduler |
   +-------------+     +----------+
          |
   +------v------+
   |     FS      |     +----------+
   |   fs.c      |     |   V6F   |  int32_t[6] fixed-point
   +-------------+     |  v6f.c  |
                       +----------+
```

---

## Mode Configuration Table

| Module | DK-2 (Integer) | DK-1 (Tick Boundary) | Notes |
|--------|:-:|:-:|-------|
| canvas.c | OK | OK | energy increment per tick |
| gate.c | OK | OK | lock/unlock per tick |
| scan.c | OK | OK | stateless |
| pattern.c | OK | N/A | spatial: uint8_t scores; lang: uint16_t conf |
| constellation.c | OK | OK | uint8_t energy, int propagation |
| emotion.c | OK | OK | uint8_t fields, integer EMA decay |
| stream.c | OK | OK | keyframe + delta, integer only |
| compress.c | OK | OK | n-gram + ELO, integer weights |
| elo.c | OK | OK | cascade at fixed tick intervals |
| v6f.c | OK | N/A | int32_t fixed-point x100 |
| wh.c | OK | OK | uint8_t metadata |
| bh.c | OK | OK | uint8_t energy_avg |
| merge.c | OK | N/A | bitwise ops only |
| lane.c | OK | OK | tick-driven scheduling |
| branch.c | OK | N/A | COW patches |
| multiverse.c | OK | N/A | uint16_t probability |
| scheduler.c | OK | OK | round-robin by tick |
| chat.c | OK | N/A | word + byte n-gram generation |
| fs.c | OK | N/A | simple uint8_t storage |

---

## Selection Rationale

| Decision | Chosen | Alternative | Reason |
|----------|--------|-------------|--------|
| Emotion precision | uint8_t (0-255) | uint16_t | 7 emotions x 1B = 7B fits cache line. 256 levels sufficient for Plutchik model |
| V6F storage | int32_t x100 | int64_t | 32-bit covers -$21M to +$21M at cent precision. No overflow in typical finance |
| Probability | uint16_t (0-65535) | uint32_t | 16 universes max, 65535 granularity sufficient for ranking |
| N-gram hash | FNV-1a 16-bit | CRC32 | FNV-1a is faster, 65536 buckets balance memory vs collision |
| ELO cascade | 3 layers | 2 or 4 | 3 maps to second/minute/hour analogy. Diminishing returns beyond 3 |
| Compress format | dual auto-select | single format | Corrections efficient for predictable data, XOR for random. Auto-select always wins |
| Chat generation | word + byte 2-stage | byte-only | Word-level captures semantic structure; byte fallback ensures always generates |
| Distance metric | squared integer | isqrt | Squared distance preserves ordering without expensive sqrt |

---

## Test Coverage

| Test Suite | Assertions | Coverage |
|------------|----------:|---------|
| test_canvas | 30 | cell CRUD, tick, bounds |
| test_gate | 30 | lock, unlock, region isolation |
| test_scan | 30 | ring, spiral, find_pattern |
| test_pattern | 30 | 6 spatial layers, training |
| test_constellation | 30 | build, propagate, infer, update |
| test_emotion | 30 | update, blend, dominant, to_energy |
| test_stream | 30 | keyframe, delta, reconstruct |
| test_compress | 30 | round-trip, ratio, edge cases |
| test_v6f | 30 | encode/decode, distance, similarity |
| test_wh | 30 | record, metadata, count |
| test_bh | 30 | compress_history, forget, summarize |
| test_merge | 30 | 6 policies, delta, conflict |
| test_lane | 15 | spawn, tick, kill |
| test_branch | 30 | create, patch, merge, delete |
| test_multiverse | 30 | spawn, collapse, probability |
| test_scheduler | 15 | spawn, tick, kill |
| **test_elo** | **20** | trust, predict, feed, cascade |
| **test_constellation_emotion** | **10** | apply_emotion JOY/FEAR |
| **test_lang_pattern** | **20** | train, predict, confidence |
| **test_chat** | **20** | train, generate, word count |
| **test_dk2_compliance** | **15** | struct sizes, saturation |
| **Total** | **505** | |
