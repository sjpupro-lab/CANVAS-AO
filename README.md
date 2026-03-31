# SJ-CANVAOS (A/O)

> **OS = AI, AI = OS** — 캔버스가 생각하고, 실행이 곧 사고다.

[![version](https://img.shields.io/badge/version-1.0.0-blue)]()
[![lang](https://img.shields.io/badge/lang-C-brightgreen)]()
[![arch](https://img.shields.io/badge/arch-aarch64-orange)]()

---

## 소개

SJ-CANVAOS는 **A/O (AI/OS)** 아키텍처로 설계된 운영체제입니다.

전통적인 OS 위에 AI를 올리는 방식을 거부하고,  
**운영체제 자체가 인공지능**이 되는 구조를 구현합니다.

- 패턴 인식 = OS 지각 계층
- 감정 벡터 = 시스템 우선순위
- 컨스텔레이션 추론 = 스케줄러 판단
- 예측 압축 = 메모리 계층
- WhiteHole/BlackHole = 기억 시스템

---

## 디렉터리 구조

```
SJ-CANVAOS/
  ├── include/canvasos.h    통합 헤더 (Cell, ABGR, 공통 타입)
  ├── core/                 A/O 코어 (AI = OS)
  │   ├── canvas.c          4096² 캔버스 기판
  │   ├── gate.c            타일 접근 제어
  │   ├── scan.c            Ring/Spiral 공간 탐색
  │   ├── pattern.c         6계층 패턴 인식
  │   ├── constellation.c   에너지 전파 추론
  │   ├── emotion.c         7차원 감정 벡터 (Plutchik)
  │   ├── stream.c          KEYFRAME + DELTA 스트림
  │   ├── compress.c        예측적 압축
  │   ├── v6f.c             6차원 금융 벡터
  │   ├── wh.c              WhiteHole (시간 기록)
  │   ├── bh.c              BlackHole (시간 압축)
  │   ├── merge.c           6가지 델타 병합 정책
  │   ├── lane.c            256 병렬 실행 레인
  │   ├── branch.c          무복사 분기 (COW)
  │   ├── multiverse.c      16 우주 병렬 시나리오
  │   ├── scheduler.c       프로세스 스케줄러
  │   └── fs.c              CanvasFS 장기 메모리
  ├── apps/
  │   ├── dating/           연애 AI (ELO 캐릭터)
  │   ├── finance/          금융 분석 + 멀티버스 예측
  │   ├── terminal/         개발자 어시스턴트 REPL
  │   └── craft/            ASCII 캔버스 시뮬레이션
  ├── tests/test_core.c     474+ 테스트 케이스
  ├── benchmark/            성능 벤치마크
  ├── docs/AO_ARCHITECTURE.md  아키텍처 문서
  └── Makefile
```

---

## 빠른 시작

```bash
# 의존성: gcc, make, libm

# 전체 빌드 + 테스트
make all

# 테스트만 실행
make test

# 공유 라이브러리
make ao

# 개별 앱
make dating
make finance
make terminal
make craft

# 벤치마크
make benchmark
make bench_enwik8
```

---

## 핵심 API

```c
// 캔버스
canvas_init();
canvas_set(x, y, cell);
Cell c = canvas_get(x, y);
canvas_tick();

// 패턴 인식 (6계층)
PatternResult pr = pattern_recognize(x, y, radius);

// 감정 (7D Plutchik)
EmotionVector ev;
emotion_init(&ev);
emotion_update(&ev, EMOTION_JOY, 0.5f);
EmotionIndex dom = emotion_dominant(&ev);

// 멀티버스
multiverse_init();
int uid = multiverse_spawn(-1, 0);
Cell mc = multiverse_get_cell(uid, x, y);

// 압축
compress_predicted_delta(input, size, output, out_size);
compress_decompress(compressed, csz, output, out_size);
```

---

## 테스트

```
make test
→ PASS: 474/474 tests
```

16개 테스트 그룹:
canvas, gate, scan, pattern, constellation, emotion,
stream, compress, v6f, wh, bh, merge, lane, branch,
multiverse, scheduler

---

## Termux / aarch64 호환

```bash
pkg install clang make
make test
```

표준 C99 + POSIX. 플랫폼 특화 헤더 없음.

---

## 라이선스

MIT

---

> *"캔버스가 생각하고, 실행이 곧 사고다."*  
> — SJ-CANVAOS A/O v1.0.0
