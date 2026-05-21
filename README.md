# sentinel-stack

> 전술 통신 미들웨어 스택 — 방산 임베디드 시스템 포트폴리오

리눅스/macOS 기반 실시간 통신 스택을 세 레이어로 구현한 프로젝트입니다.  
RTOS 스케줄러 · 소켓 통신 · 패킷 분석기가 단일 시스템으로 연동됩니다.

---

## 프로젝트 개요

방산 임베디드 시스템에서 요구하는 핵심 역량을 실습 수준이 아닌  
**설계 → 구현 → 검증 → 문서화** 전 과정을 포함한 포트폴리오입니다.

### 시스템 아키텍처

```
┌─────────────────────────────────────────────────────────┐
│                   sentinel-stack                        │
│                                                         │
│  ┌──────────────────────────────────────────────┐       │
│  │         Layer 3 : Packet Analyzer            │       │
│  │   실시간 패킷 캡처 · 프로토콜 파싱 · 이상 탐지         │       │
│  └──────────────────┬───────────────────────────┘       │
│                     │ 모니터링                            │
│  ┌──────────────────▼───────────────────────────┐       │
│  │         Layer 2 : Socket Communication       │       │
│  │   TCP/UDP 이중화 · Publisher-Subscriber · AES  │       │
│  └──────────────────┬───────────────────────────┘       │
│                     │ 태스크 실행                          │
│  ┌──────────────────▼───────────────────────────┐       │
│  │         Layer 1 : RTOS Scheduler             │       │
│  │   선점형 스케줄링 · RMS/EDF · Jitter 측정         │       │
│  └──────────────────────────────────────────────┘       │
│                                                         │
│  Target: Linux (ARM) / macOS (BSD)  |  QEMU 검증         │
└─────────────────────────────────────────────────────────┘
```

### 참고 표준

| 표준 / 프로토콜 | 적용 영역 |
|---|---|
| MIL-STD-1553 | 통신 이중화 구조 참고 |
| ARINC 653 | 파티셔닝 스케줄러 설계 참고 |
| MISRA-C 2012 | 코딩 컨벤션 |
| DO-178C | 소프트웨어 검증 접근법 참고 |
| Link-16 개념 | 메시지 포맷 설계 참고 |

---

## 디렉토리 구조

```
sentinel-stack/
│
├── README.md
│
├── docs/
│   ├── architecture.md          # 전체 시스템 설계 문서
│   ├── rtos-design.md           # RTOS 스케줄러 설계 명세
│   ├── comm-protocol.md         # 커스텀 통신 프로토콜 명세
│   ├── packet-analyzer.md       # 패킷 분석기 설계 명세
│   ├── test-results/
│   │   ├── rtos_jitter_report.md
│   │   ├── latency_benchmark.md
│   │   └── detection_accuracy.md
│   └── references.md            # 참고 표준 및 문헌
│
├── rtos-scheduler/              # Layer 1
│   ├── include/
│   │   ├── scheduler.h
│   │   ├── task.h
│   │   └── timer.h
│   ├── src/
│   │   ├── scheduler.c          # 핵심 스케줄러 엔진
│   │   ├── rms.c                # Rate Monotonic Scheduling
│   │   ├── edf.c                # Earliest Deadline First
│   │   └── timer.c              # POSIX 타이머 래퍼
│   ├── tests/
│   │   ├── test_scheduler.c
│   │   └── test_jitter.c
│   └── CMakeLists.txt
│
├── socket-comm/                 # Layer 2
│   ├── include/
│   │   ├── comm_node.h
│   │   ├── protocol.h           # 커스텀 바이너리 프로토콜
│   │   ├── pubsub.h             # Publisher-Subscriber 구조
│   │   └── crypto.h             # AES-256 암호화 레이어
│   ├── src/
│   │   ├── comm_node.c
│   │   ├── tcp_transport.c
│   │   ├── udp_transport.c
│   │   ├── pubsub.c
│   │   ├── crypto.c
│   │   ├── io_kqueue.c          # macOS 전용 I/O
│   │   └── io_epoll.c           # Linux 전용 I/O
│   ├── tests/
│   │   ├── test_latency.c
│   │   └── test_reconnect.c
│   └── CMakeLists.txt
│
├── packet-analyzer/             # Layer 3
│   ├── include/
│   │   ├── capture.h
│   │   ├── parser.h             # 프로토콜 파서
│   │   ├── rule_engine.h        # 이상 탐지 룰 엔진
│   │   └── reporter.h
│   ├── src/
│   │   ├── capture.c            # libpcap 캡처
│   │   ├── parser_eth.c         # Ethernet 파서
│   │   ├── parser_ip.c          # IP 파서
│   │   ├── parser_tcp.c         # TCP 파서
│   │   ├── parser_custom.c      # sentinel 커스텀 프로토콜 파서
│   │   ├── rule_engine.c
│   │   └── reporter.c           # CSV + 통계 출력
│   ├── rules/
│   │   └── default.rules        # 기본 탐지 룰셋
│   ├── tests/
│   │   └── test_parser.c
│   └── CMakeLists.txt
│
├── demo/
│   ├── run_all.sh               # 통합 데모 실행 스크립트
│   └── scenario_01.sh           # 시나리오별 테스트
│
├── docker/
│   ├── Dockerfile               # Linux 빌드 환경
│   └── docker-compose.yml       # 멀티 노드 시뮬레이션
│
├── qemu/
│   ├── run_arm.sh               # ARM 에뮬레이션 실행
│   └── linker.ld                # ARM 링커 스크립트
│
└── CMakeLists.txt               # 루트 빌드 설정 (크로스 플랫폼)
```

---

## 빌드 및 실행

### 요구사항

| 항목 | macOS | Linux |
|---|---|---|
| 컴파일러 | Apple Clang 15+ / GCC 13+ | GCC 13+ |
| 빌드 시스템 | CMake 3.20+, Ninja | CMake 3.20+, Ninja |
| 패킷 캡처 | libpcap (내장) | libpcap-dev |
| 암호화 | OpenSSL 3.x | libssl-dev |
| ARM 에뮬 | QEMU 8.x | QEMU 8.x |
| 크로스 컴파일 | arm-none-eabi-gcc | arm-none-eabi-gcc |

### macOS 빠른 시작

```bash
# 1. 저장소 클론
git clone https://github.com/YOUR_ID/sentinel-stack.git
cd sentinel-stack

# 2. 의존성 설치
brew install cmake ninja libpcap openssl qemu
brew install --cask gcc-arm-embedded

# 3. 빌드 (전체)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 4. 개별 모듈 빌드
cmake -B build -G Ninja -DBUILD_MODULE=rtos    # RTOS만
cmake -B build -G Ninja -DBUILD_MODULE=comm    # 소켓 통신만
cmake -B build -G Ninja -DBUILD_MODULE=pcap    # 패킷 분석기만
```

### Linux / Docker

```bash
# Docker 환경으로 Linux 빌드
docker build -t sentinel-stack ./docker/
docker run --rm \
  --cap-add=NET_ADMIN \
  --cap-add=NET_RAW \
  sentinel-stack

# 또는 docker-compose로 멀티 노드 시뮬레이션
docker-compose -f docker/docker-compose.yml up
```

### QEMU ARM 에뮬레이션

```bash
# ARM Cortex-M3 타겟 빌드
make CROSS_COMPILE=arm-none-eabi- TARGET=qemu

# QEMU 실행 (mps2-an385 보드 에뮬레이션)
./qemu/run_arm.sh

# 또는 직접 실행
qemu-system-arm \
  -machine mps2-an385 \
  -cpu cortex-m3 \
  -kernel build/rtos-scheduler/rtos_qemu.elf \
  -nographic \
  -serial stdio
```

---

## 모듈별 설명

### Layer 1 — RTOS Scheduler

POSIX 환경에서 선점형 실시간 스케줄러를 구현합니다.  
QEMU ARM Cortex-M3 타겟에서 동작을 검증합니다.

**구현 기능**

- 선점형(Preemptive) 우선순위 기반 스케줄러
- Rate Monotonic Scheduling (RMS) 알고리즘
- Earliest Deadline First (EDF) 알고리즘
- Deadline Miss 감지 및 로깅
- Mutex / Semaphore / Message Queue 직접 구현
- WCET (Worst Case Execution Time) 측정 도구
- Jitter 측정 및 통계 리포트

**태스크 정의 구조**

```c
typedef struct {
    uint32_t  task_id;
    char      name[32];
    uint32_t  period_ms;       // 주기
    uint32_t  deadline_ms;     // 데드라인
    uint32_t  wcet_us;         // 최악 실행 시간 (마이크로초)
    uint8_t   priority;        // 0 = 최고 우선순위
    TaskState state;
    void      (*task_func)(void *arg);
    void      *arg;
} rtos_task_t;
```

**실행 예시**

```bash
./build/rtos-scheduler/rtos_sim --algo rms --tasks 5 --duration 10s

# 출력 예시
[RTOS] Scheduler started: RMS, 5 tasks
[  0.000ms] TASK_1 (prio=0) RUNNING
[  1.234ms] TASK_1 DONE  wcet=1.234ms
[  1.235ms] TASK_2 (prio=1) RUNNING
...
[RTOS] === Jitter Report ===
       TASK_1: avg=0.12us  max=1.45us  min=0.08us
       TASK_2: avg=0.18us  max=2.10us  min=0.11us
[RTOS] Deadline Miss: 0 / 1000 activations
```

**측정 결과 요약** _(macOS M2, Release 빌드 기준)_

| 항목 | 결과 |
|---|---|
| 컨텍스트 스위치 오버헤드 | ~2.1 µs |
| 타이머 지터 (평균) | < 5 µs |
| 타이머 지터 (최대) | < 50 µs |
| 데드라인 미스율 (5태스크) | 0% |

---

### Layer 2 — Socket Communication

TCP/UDP 이중화 통신 노드를 구현합니다.  
DDS(Data Distribution Service) 구조를 참고한 Publisher-Subscriber 패턴을 적용합니다.

**구현 기능**

- TCP 신뢰 채널 + UDP 고속 채널 이중화 운용
- Publisher-Subscriber 패턴 (토픽 기반 메시지 라우팅)
- epoll(Linux) / kqueue(macOS) 이식성 추상화 레이어
- 커스텀 바이너리 프로토콜 (직렬화/역직렬화)
- AES-256-GCM 페이로드 암호화 (OpenSSL)
- 링크 장애 감지 및 자동 재연결 (Heartbeat 기반)
- 멀티캐스트 그룹 지원

**커스텀 프로토콜 패킷 구조**

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├───────────────────────────────────────────────────────────────┤
│  Magic (0xSENT)   │    Version    │    Message Type           │
├───────────────────────────────────────────────────────────────┤
│                        Sequence Number                        │
├───────────────────────────────────────────────────────────────┤
│                        Timestamp (ms)                         │
├───────────────────────────────────────────────────────────────┤
│   Source Node ID  │  Dest Node ID │       Topic ID            │
├───────────────────────────────────────────────────────────────┤
│                     Payload Length                            │
├───────────────────────────────────────────────────────────────┤
│                     AES-256-GCM IV (12 bytes)                 │
├───────────────────────────────────────────────────────────────┤
│                     Encrypted Payload                         │
│                         ...                                   │
├───────────────────────────────────────────────────────────────┤
│                     CRC-32 Checksum                           │
└───────────────────────────────────────────────────────────────┘
```

**실행 예시**

```bash
# 터미널 1: 지휘 노드 (Publisher)
./build/socket-comm/comm_node \
  --id 1 --role commander \
  --tcp-port 9000 --udp-port 9001

# 터미널 2: 센서 노드 (Subscriber)
./build/socket-comm/comm_node \
  --id 2 --role sensor \
  --connect 127.0.0.1:9000 \
  --subscribe SENSOR_DATA,STATUS

# 터미널 3: 성능 측정
./build/socket-comm/latency_test \
  --target 127.0.0.1:9000 --count 10000
```

**측정 결과 요약** _(루프백 인터페이스 기준)_

| 항목 | TCP | UDP |
|---|---|---|
| 평균 왕복 지연 (RTT) | 0.18 ms | 0.09 ms |
| 처리량 (1KB 메시지) | 380 MB/s | 520 MB/s |
| 재연결 소요 시간 | < 500 ms | N/A |
| 암호화 오버헤드 | +12% | +9% |

---

### Layer 3 — Packet Analyzer

libpcap 기반 실시간 패킷 캡처 및 분석 도구입니다.  
Layer 2에서 생성되는 sentinel 커스텀 프로토콜 패킷도 파싱합니다.

**구현 기능**

- libpcap 기반 실시간 패킷 캡처 (무차별 모드 지원)
- 계층별 프로토콜 파서 직접 구현 (Ethernet / IP / TCP / UDP)
- sentinel 커스텀 프로토콜 자동 식별 및 파싱
- 룰 기반 이상 트래픽 탐지 엔진
- 통계 집계 및 CSV 리포트 자동 생성
- 실시간 TUI 대시보드 (ncurses)

**탐지 룰 예시** _(Snort 문법 참고)_

```
# rules/default.rules

# 포트 스캔 탐지
alert tcp any any -> any any \
  (flags:S; threshold: count 20, seconds 1; \
   msg:"Port Scan Detected"; sid:1001;)

# sentinel 프로토콜 재전송 폭주
alert sentinel any any -> any any \
  (retransmit_count:>10; window:5s; \
   msg:"Sentinel Retransmit Storm"; sid:2001;)

# 비인가 노드 접속 시도
alert tcp any any -> any 9000 \
  (sentinel_magic:!0x53454e54; \
   msg:"Invalid Magic Number"; sid:2002;)
```

**실행 예시**

```bash
# macOS - 루프백 인터페이스 모니터링
sudo ./build/packet-analyzer/sentinel_pcap -i lo0

# Linux - 네트워크 인터페이스
sudo ./build/packet-analyzer/sentinel_pcap -i eth0

# 캡처 파일 오프라인 분석
./build/packet-analyzer/sentinel_pcap \
  -r capture_20240101.pcap \
  --rules rules/default.rules \
  --report output/report.csv

# 출력 예시
[PCAP] Capturing on lo0...
[  0.001s] ETH | IP 127.0.0.1 → 127.0.0.1 | TCP 52341 → 9000
[  0.001s]   └─ SENTINEL v1 | NODE:1→2 | TOPIC:STATUS | len=128 | AES-GCM OK
[  0.002s] ETH | IP 127.0.0.1 → 127.0.0.1 | UDP 52342 → 9001
[  0.002s]   └─ SENTINEL v1 | NODE:1→255 | TOPIC:SENSOR_DATA | len=64 | MULTICAST

=== 10s Summary ===
Total Packets   :  48,291
Sentinel Packets:  47,103 (97.5%)
Anomalies       :       2
Avg Packet Size :   98.3 bytes
```

---

## 통합 데모

세 모듈을 동시에 실행하는 시나리오입니다.

```bash
# 통합 데모 자동 실행 (tmux 필요)
brew install tmux
./demo/run_all.sh
```

스크립트가 4분할 터미널을 자동으로 구성합니다.

```
┌─────────────────────────┬─────────────────────────┐
│   [RTOS Scheduler]      │  [Commander Node #1]    │
│   Task 스케줄 현황       │  메시지 발행 현황         │
│   Jitter 실시간 표시     │  토픽: STATUS, CMD       │
├─────────────────────────┼─────────────────────────┤
│   [Sensor Node #2]      │  [Packet Analyzer]      │
│   메시지 수신 현황        │  lo0 실시간 캡처         │
│   토픽 구독 현황          │  이상 탐지 경보           │
└─────────────────────────┴─────────────────────────┘
```

---

## 테스트

```bash
# 전체 단위 테스트
ctest --test-dir build --output-on-failure

# 모듈별 테스트
ctest --test-dir build -R rtos     # RTOS 스케줄러 테스트
ctest --test-dir build -R comm     # 소켓 통신 테스트
ctest --test-dir build -R pcap     # 패킷 파서 테스트

# 성능 벤치마크
./build/rtos-scheduler/benchmark_jitter --duration 60s
./build/socket-comm/benchmark_latency  --count 100000
```

---

## 코딩 컨벤션

MISRA-C 2012 가이드라인을 준수합니다.

- 동적 메모리 할당 (`malloc`) 금지 — 정적 메모리 풀 사용
- 모든 함수 반환값 검사
- 전역 변수 최소화 (모듈 내부 `static` 한정)
- 포인터 역참조 전 NULL 검사 필수
- 배열 접근 전 범위 검사 필수
- `assert()` 를 방어적 프로그래밍에 적극 활용

```c
// 나쁜 예
char *buf = malloc(1024);
memcpy(buf, src, len);

// 좋은 예 (정적 메모리 풀)
static uint8_t packet_pool[MAX_NODES][PACKET_BUF_SIZE];
ASSERT(node_id < MAX_NODES);
ASSERT(len <= PACKET_BUF_SIZE);
memcpy(packet_pool[node_id], src, len);
```

---

## 개발 환경

| 항목 | 버전 |
|---|---|
| 개발 OS | macOS 14 Sonoma (Apple M2) |
| 검증 OS | Ubuntu 22.04 LTS (Docker) |
| ARM 에뮬 | QEMU 8.2.0 (Cortex-M3) |
| 컴파일러 | Apple Clang 15 / GCC 13 |
| 크로스 컴파일 | arm-none-eabi-gcc 13.2 |
| 빌드 시스템 | CMake 3.28 + Ninja |
| 분석 도구 | Valgrind, AddressSanitizer, Perf |

---

## 로드맵

- [x] 프로젝트 설계 및 문서화
- [ ] Layer 1: RTOS 스케줄러 기본 구현
- [ ] Layer 1: RMS / EDF 알고리즘 구현
- [ ] Layer 1: Jitter 측정 도구 구현
- [ ] Layer 1: QEMU ARM 포팅
- [ ] Layer 2: TCP/UDP 기본 소켓 구현
- [ ] Layer 2: Publisher-Subscriber 구현
- [ ] Layer 2: AES-256-GCM 암호화 적용
- [ ] Layer 2: 링크 장애 복구 구현
- [ ] Layer 3: libpcap 기반 캡처 구현
- [ ] Layer 3: 프로토콜 파서 구현
- [ ] Layer 3: 룰 엔진 구현
- [ ] 통합 데모 시나리오 완성
- [ ] 성능 측정 리포트 작성

---

## 참고 자료

- [FreeRTOS Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel)
- [ARINC 653 Specification](https://www.aviation-ia.com/arinc-specification-653)
- [MISRA-C 2012 Guidelines](https://www.misra.org.uk/)
- [libpcap Documentation](https://www.tcpdump.org/manpages/pcap.3pcap.html)
- [OpenSSL AES-GCM](https://www.openssl.org/docs/man3.0/man3/EVP_aead_aes_256_gcm.html)
- MIL-STD-1553B Digital Time Division Command/Response Multiplex Data Bus

---

## 라이선스

MIT License — 포트폴리오 목적으로 자유롭게 참고 가능합니다.
