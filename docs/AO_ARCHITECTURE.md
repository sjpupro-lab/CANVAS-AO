# SJ-CANVAOS A/O 아키텍처

## A/O = AI/OS: 캔버스가 생각하고, 실행이 곧 사고다

---

## 1. 철학: OS = AI, AI = OS

SJ-CANVAOS의 핵심 철학은 **운영체제와 인공지능을 분리하지 않는다**는 것입니다.

전통적인 시스템에서 AI는 OS 위에 올라가는 *애플리케이션*입니다.
A/O 아키텍처에서 AI **는** OS입니다.

- 패턴 인식(`pattern.c`) → OS의 지각 계층
- 감정 벡터(`emotion.c`) → OS의 상태 관리
- 컨스텔레이션 추론(`constellation.c`) → OS의 스케줄러 판단
- 압축(`compress.c`) → OS의 메모리 계층
- 스트림(`stream.c`) → OS의 I/O 서브시스템

이 모든 것은 별도 AI 모듈이 아닙니다. **코어 시스템 함수**입니다.

---

## 2. 캔버스 기판 (canvas.c)

```
Cell canvas[4096][4096]
```

- 4096×4096 셀 배열이 전체 시스템의 기반 메모리입니다.
- 각 셀은 `ABGR(색상)` + `energy(에너지)` + `state(상태)` + `id(식별자)`로 구성됩니다.
- `canvas_tick()`은 한 클럭 사이클을 진행시킵니다. 실행 = 사고.

### Cell 구조

| 필드    | 타입     | 의미                          |
|---------|----------|-------------------------------|
| color   | ABGR     | 4채널 색상 (Alpha/Blue/Green/Red) |
| energy  | uint8_t  | 셀의 활성 에너지 (0-255)       |
| state   | uint8_t  | IDLE / ACTIVE / LOCKED / DEAD |
| id      | uint16_t | 셀 식별자                     |

---

## 3. 코어 모듈 상세

### 3.1 Gate (gate.c) — 타일 접근 제어

4096/64 = 64×64 타일 단위로 영역을 잠그고 소유권을 부여합니다.
AI 에이전트가 자신의 작업 영역을 격리할 때 사용합니다.

### 3.2 Scan (scan.c) — 공간 탐색

- `scan_ring()`: 중심에서 반경 r의 둘레를 탐색
- `scan_spiral()`: 나선형으로 확장하며 탐색
- AI가 컨텍스트를 수집하는 방식 = OS가 메모리를 탐색하는 방식

### 3.3 Pattern (pattern.c) — 6계층 패턴 인식

```
LAYER_RAW → LAYER_EDGE → LAYER_SHAPE →
LAYER_OBJECT → LAYER_CONTEXT → LAYER_ABSTRACT
```

하위 계층에서 상위 개념으로 추상화됩니다.
이것은 OS의 디바이스 드라이버 → 파일시스템 → 애플리케이션 계층과 동형입니다.

### 3.4 Constellation (constellation.c) — 에너지 전파 추론

셀들 사이의 에너지 흐름을 그래프로 모델링하여 다음 상태를 추론합니다.
기존 OS의 프리페치/예측 로직에 해당합니다.

### 3.5 Emotion (emotion.c) — 7차원 감정 벡터

Plutchik의 감정 모델을 따르는 7D 벡터:
`joy, trust, fear, surprise, sadness, disgust, anger`

OS의 관점에서 이것은 **시스템 우선순위 바이어스**입니다.
감정이 높은 채널에 더 많은 에너지가 흐릅니다.

### 3.6 Stream (stream.c) — KEYFRAME + DELTA

- `KEYFRAME`: 전체 상태 스냅샷 (16프레임마다)
- `DELTA`: 이전 프레임과의 차분

비디오 코덱과 동일한 원리. OS의 I/O 버퍼링 전략.

### 3.7 Compress (compress.c) — 예측적 압축

8바이트 컨텍스트를 기반으로 다음 바이트를 예측하고 XOR 델타만 저장합니다.
예측이 정확할수록 압축률이 높아집니다 = AI가 더 똑똑할수록 OS가 더 효율적.

### 3.8 V6F (v6f.c) — 6차원 금융 벡터

`[price, open, high, low, volume, time]`을 하나의 벡터로 인코딩합니다.
캔버스 좌표로 매핑하여 공간적 패턴 인식을 적용합니다.

### 3.9 WhiteHole / BlackHole (wh.c / bh.c)

- **WhiteHole**: 시간을 기록합니다. 모든 이벤트를 타임스탬프와 함께 저장.
- **BlackHole**: 오래된 기록을 압축/요약하여 망각합니다.
  
인간의 기억처럼: 중요한 것은 유지, 덜 중요한 것은 요약, 무관한 것은 삭제.

### 3.10 Merge (merge.c) — 6가지 병합 정책

| 정책         | 설명                          |
|--------------|-------------------------------|
| MERGE_LATEST | 최신 값 채택                  |
| MERGE_OLDEST | 최초 값 유지                  |
| MERGE_AND    | 비트 AND (교집합)             |
| MERGE_OR     | 비트 OR (합집합)              |
| MERGE_XOR    | 비트 XOR (차이)               |
| MERGE_AVERAGE| 평균값                        |

### 3.11 Lane (lane.c) — 256 병렬 실행 레인

256개의 독립 실행 채널. OS 스레드 풀의 A/O 구현.

### 3.12 Branch (branch.c) — 무복사 분기

Copy-On-Write 패치 방식으로 캔버스를 복사 없이 분기합니다.
Git의 브랜치와 동일한 원리.

### 3.13 Multiverse (multiverse.c) — 16개 우주

16개의 병렬 시나리오를 동시에 유지하며 확률로 관리합니다.
베이지안 추론의 OS 구현.

### 3.14 Scheduler (scheduler.c) — 프로세스 스케줄링

라운드로빈 기반 스케줄러. Lane 위에서 Process를 관리합니다.

### 3.15 CanvasFS (fs.c) — 장기 메모리 파일시스템

키-값 기반 파일시스템. OS의 디스크 = A/O의 장기 기억.

---

## 4. 애플리케이션 계층

### 4.1 Dating Engine (apps/dating/)

- `persona.c`: ELO 캐릭터 — 7차원 성격 벡터
- `dating_engine.c`: 대화 → 감정 업데이트 → WH 기록 → 응답 선택
- A/O 코어가 감정 상태를 관리하므로 자연스러운 인격 시뮬레이션

### 4.2 Finance (apps/finance/)

- `finance_bench.c`: V6F 가격 데이터 → 캔버스 → 패턴 인식
- `multiverse_bench.c`: 16개 시나리오 동시 분석 → 확률 기반 예측
- `anomaly.c`: Z-score + 감정(시장심리) + 컨스텔레이션으로 이상 탐지

### 4.3 Terminal (apps/terminal/)

- 개발자 어시스턴트 REPL
- 입력 → 캔버스 매핑 → 패턴 인식 → 제안 생성

### 4.4 CanvasCraft (apps/craft/)

- 캔버스를 ASCII로 시각화하는 시뮬레이션 세계
- `canvas_tick()`이 세계를 진행시킴

---

## 5. 빌드

```bash
# 전체 빌드
make all

# 공유 라이브러리만
make ao

# 테스트 실행
make test

# 개별 앱
make dating
make finance
make terminal
make craft
make benchmark
make bench_enwik8
```

---

## 6. 설계 원칙 요약

1. **캔버스가 메모리다** — 4096² 셀 = 주소 공간
2. **에너지가 실행이다** — energy > 0 인 셀이 연산을 수행
3. **패턴이 인식이다** — 6계층 추상화가 OS의 지각 계층
4. **감정이 우선순위다** — 7D 벡터가 자원 배분을 결정
5. **분기가 사고다** — 무복사 Branch + Multiverse = 가설 검증
6. **압축이 기억이다** — 예측 압축 + WH/BH = 기억 계층

> **A/O = AI/OS: 캔버스가 생각하고, 실행이 곧 사고다.**
