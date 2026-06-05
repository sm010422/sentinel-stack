# sentinel-stack 실행 가이드

> 빌드부터 통합 데모까지 단계별 실행 방법을 설명합니다.

---

## 목차

1. [사전 요구사항](#1-사전-요구사항)
2. [빌드](#2-빌드)
3. [단위 테스트](#3-단위-테스트)
4. [모듈별 실행](#4-모듈별-실행)
   - [Layer 1 — RTOS 스케줄러](#layer-1--rtos-스케줄러)
   - [Layer 2 — 소켓 통신 노드](#layer-2--소켓-통신-노드)
   - [Layer 3 — 패킷 분석기](#layer-3--패킷-분석기)
   - [Layer 4 — HAL 루프백 테스트](#layer-4--hal-루프백-테스트)
   - [Layer 5 — HIL 시뮬레이터](#layer-5--hil-시뮬레이터)
5. [통합 데모](#5-통합-데모)
6. [시나리오 — SYN Flood 탐지](#6-시나리오--syn-flood-탐지)
7. [정적·동적 코드 분석](#7-정적동적-코드-분석)
8. [Docker 멀티 노드 시뮬레이션](#8-docker-멀티-노드-시뮬레이션)
9. [QEMU ARM 에뮬레이션](#9-qemu-arm-에뮬레이션)

---

## 1. 사전 요구사항

### macOS

```bash
brew install cmake openssl libpcap tmux
```

| 도구 | 최소 버전 | 용도 |
|------|-----------|------|
| CMake | 3.20+ | 빌드 시스템 |
| Apple Clang / GCC | 14+ | C11 컴파일러 |
| OpenSSL | 3.x | AES-256-GCM 암호화 |
| libpcap | 1.10+ | 패킷 캡처 (Layer 3) |
| tmux | 3.x | 통합 데모 분할 터미널 |

정적 분석 도구 (선택):

```bash
brew install cppcheck llvm   # clang-tidy 포함
```

### Linux (Ubuntu 22.04)

```bash
sudo apt-get install build-essential cmake libssl-dev libpcap-dev tmux
```

---

## 2. 빌드

### 전체 빌드 (권장)

```bash
git clone <repo-url>
cd sentinel-stack

cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

빌드 후 생성되는 바이너리:

```
build/
├── rtos-scheduler/
│   ├── rtos_sim          ← RTOS 스케줄러 시뮬레이터
│   ├── benchmark_jitter  ← 지터 벤치마크
│   └── test_rtos         ← 단위 테스트
├── socket-comm/
│   ├── comm_node         ← 통신 노드 (지휘소/센서)
│   ├── latency_test      ← 지연 시간 측정
│   └── test_reconnect    ← 재연결 테스트
├── packet-analyzer/
│   ├── sentinel_pcap     ← 패킷 캡처·분석기
│   └── test_parser       ← 파서 단위 테스트
├── hal/
│   └── test_hal_loopback ← HAL 루프백 테스트
└── hil-simulator/
    └── hil_demo          ← 6-DOF IMU HIL 데모
```

### 모듈별 빌드

특정 레이어만 빌드하려면 `BUILD_MODULE` 옵션을 사용합니다.

```bash
cmake -B build -DBUILD_MODULE=rtos   # Layer 1만
cmake -B build -DBUILD_MODULE=comm   # Layer 2만
cmake -B build -DBUILD_MODULE=pcap   # Layer 3만
cmake -B build -DBUILD_MODULE=hal    # Layer 4만
cmake -B build -DBUILD_MODULE=hil    # Layer 5만
cmake -B build -DBUILD_MODULE=all    # 전체 (기본값)
cmake --build build
```

### Debug 빌드 (AddressSanitizer 포함)

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Debug 빌드는 `-fsanitize=address,undefined` 플래그가 자동 적용됩니다.

---

## 3. 단위 테스트

```bash
ctest --test-dir build --output-on-failure
```

출력 예시:

```
Test project /path/to/sentinel-stack/build
    Start 1: rtos_unit       Passed    0.43 sec
    Start 2: rtos_jitter     Passed    7.01 sec
    Start 3: comm_latency    Passed    0.51 sec
    Start 4: comm_reconnect  Passed    0.45 sec
    Start 5: pcap_parser     Passed    0.37 sec
    Start 6: hal_loopback    Passed    0.01 sec

100% tests passed, 0 tests failed out of 6
```

특정 모듈 테스트만 실행:

```bash
ctest --test-dir build -R rtos    # RTOS 관련 테스트만
ctest --test-dir build -R comm    # 소켓 통신 테스트만
ctest --test-dir build -R pcap    # 패킷 파서 테스트만
ctest --test-dir build -R hal     # HAL 루프백 테스트만
```

---

## 4. 모듈별 실행

### Layer 1 — RTOS 스케줄러

#### rtos_sim — 스케줄러 시뮬레이터

```bash
./build/rtos-scheduler/rtos_sim --algo rms --tasks 5 --duration 10
./build/rtos-scheduler/rtos_sim --algo edf --tasks 3 --duration 30
```

| 옵션 | 값 | 설명 |
|------|-----|------|
| `--algo` | `rms` / `edf` | 스케줄링 알고리즘 |
| `--tasks` | 1–8 | 생성할 태스크 수 |
| `--duration` | 초 단위 정수 | 시뮬레이션 실행 시간 |

출력 예시:

```
[SCHED] 태스크 등록: id=1 name='BUS_MGR' period=2ms deadline=2ms wcet=300us prio=0
[RMS] CPU 사용률: 0.4600 (한계: 0.7798, 스케줄 가능)
[RTOS] 스케줄러 시작: 알고리즘=RMS, 태스크 수=3, 실행시간=5000ms
[      0ms] BUS_MGR RUNNING (wcet=300us)
...
=== Jitter 통계 ===
  BUS_MGR : avg=+312us  max=+1051us  p99=+980us
  SENSOR  : avg=+54us   max=+200us   p99=+188us
  Deadline Miss: 0 건
```

#### benchmark_jitter — 타이머 지터 벤치마크

```bash
./build/rtos-scheduler/benchmark_jitter
```

인자 없이 실행하며 nanosleep 기반 1ms 지터를 500 샘플 측정합니다.

---

### Layer 2 — 소켓 통신 노드

두 터미널을 열어 각각 Commander(서버)와 Sensor(클라이언트)를 실행합니다.

#### 터미널 1 — Commander 노드 (TCP 서버 역할)

```bash
./build/socket-comm/comm_node \
    --id 1 \
    --role commander \
    --tcp-port 9000 \
    --udp-port 9001
```

#### 터미널 2 — Sensor 노드 (클라이언트 역할)

```bash
./build/socket-comm/comm_node \
    --id 2 \
    --role sensor \
    --connect 127.0.0.1:9000
```

| 옵션 | 설명 |
|------|------|
| `--id N` | 노드 고유 ID (1–254) |
| `--role commander` | TCP 서버 소켓 개방, 메시지 발행 |
| `--role sensor` | TCP 클라이언트로 연결, 데이터 수신 |
| `--role relay` | 메시지 중계 노드 |
| `--tcp-port P` | TCP 리슨 포트 (commander 전용) |
| `--udp-port Q` | UDP 멀티캐스트 포트 (commander 전용) |
| `--connect IP:PORT` | 연결 대상 주소 (sensor 전용) |

#### latency_test — TCP 왕복 지연 측정

Commander 노드가 실행 중인 상태에서 별도 터미널에서 실행합니다.

```bash
./build/socket-comm/latency_test
```

---

### Layer 3 — 패킷 분석기

**패킷 캡처는 root 권한이 필요합니다.**

#### 실시간 캡처 (라이브 인터페이스)

```bash
# macOS 루프백
sudo ./build/packet-analyzer/sentinel_pcap -i lo0

# Linux 루프백
sudo ./build/packet-analyzer/sentinel_pcap -i lo

# 특정 네트워크 인터페이스
sudo ./build/packet-analyzer/sentinel_pcap -i eth0
```

#### 오프라인 분석 (pcap 파일)

```bash
./build/packet-analyzer/sentinel_pcap \
    -r capture.pcap \
    --report output/report.csv
```

| 옵션 | 설명 |
|------|------|
| `-i IFACE` | 캡처할 네트워크 인터페이스 |
| `-r FILE` | 분석할 .pcap 파일 경로 |
| `--report FILE` | CSV 결과 파일 경로 |

출력 예시:

```
[PCAP] Capturing on lo0...
[  0.001s] IP 127.0.0.1 → 127.0.0.1 | TCP 52341 → 9000
[  0.001s]   └─ SENTINEL v1 | NODE:1→2 | TOPIC:STATUS | len=128 | AES-GCM OK

=== 10s 집계 ===
총 패킷    : 48,291
Sentinel   : 47,103 (97.5%)
이상 탐지  : 0
```

---

### Layer 4 — HAL 루프백 테스트

UART / RS-422 / I2C / SPI / CAN 인터페이스 시뮬레이션 테스트입니다.  
실제 하드웨어 없이 POSIX 파이프로 루프백 통신을 검증합니다.

```bash
./build/hal/test_hal_loopback
```

출력 예시:

```
[UART] 루프백 테스트...  OK
[RS-422] 루프백 테스트... OK
[I2C] 레지스터 R/W 테스트... OK
[SPI] 전이중 테스트...   OK
[CAN] 프레임 송수신 테스트... OK
모든 HAL 테스트 통과 (5/5)
```

---

### Layer 5 — HIL 시뮬레이터

6-DOF IMU 시뮬레이터입니다. I2C로 레지스터에 쓰고, 비행 컴퓨터가 읽어 CAN으로 전송하는 파이프라인을 시뮬레이션합니다.

```bash
./build/hil-simulator/hil_demo
```

인자 없이 실행하며 Ctrl+C로 종료합니다.

출력 예시:

```
════════════════════════════════════════════════════
  sentinel-stack HIL 시뮬레이터
  6-DOF IMU (ICM-42688-P) + I2C + CAN 버스
════════════════════════════════════════════════════

Time(s)  Ax(m/s²)  Ay(m/s²)  Az(m/s²)  Gx(°/s)   Gy(°/s)   Gz(°/s)    Seq
0.00     -0.0262    0.0859     9.8090    50.1644   15.8105    5.7249      0
0.50     -0.8545    0.9306     9.7243   -41.9937   -0.5375    3.1821     50
...
```

---

## 5. 통합 데모

3개 터미널에서 Layer 1–3을 동시에 실행합니다. **tmux가 필요합니다.**

```bash
brew install tmux   # 미설치 시
./demo/run_all.sh
```

스크립트가 tmux 4분할 세션을 자동 구성합니다:

```
┌─────────────────────────┬─────────────────────────┐
│   Layer 1: RTOS (RMS)   │   Layer 2: Commander    │
│   태스크 스케줄 현황      │   ID=1 / TCP:9000       │
├─────────────────────────┼─────────────────────────┤
│   Layer 2: Sensor       │   Layer 3: Packet Anal. │
│   ID=2 / 127.0.0.1:9000 │   lo0 실시간 캡처        │
└─────────────────────────┴─────────────────────────┘
```

세션 재연결:

```bash
tmux attach-session -t sentinel-demo
```

세션 종료:

```bash
tmux kill-session -t sentinel-demo
```

---

## 6. 시나리오 — SYN Flood 탐지

Layer 2 정상 통신 중 SYN Flood를 주입하고 Layer 3가 탐지하는 과정을 시연합니다.

```bash
./demo/scenario_01.sh
```

단계별 진행:

1. 패킷 분석기 백그라운드 시작
2. Commander 노드 시작
3. Sensor 노드 연결 (30초 정상 통신)
4. SYN Flood 주입 (`hping3` 또는 `nmap` 자동 사용)
5. 탐지 경보 출력 및 CSV 리포트 저장 (`output/scenario_01_*.csv`)

> `hping3` 또는 `nmap`이 없으면 정상 통신 구간만 실행됩니다.

---

## 7. 정적·동적 코드 분석

### 전체 분석 실행

```bash
./scripts/run_analysis.sh
```

3단계로 진행되며 결과를 `docs/test-results/static_analysis.md`에 저장합니다.

```
[ 1/3 ] cppcheck  — 오류: 0건 | 경고: 0건 | 스타일: 24건
[ 2/3 ] clang-tidy — 오류: 0건 | 경고: 148건
[ 3/3 ] ASan/UBSan — 실패: 0건
```

### 개별 도구 실행

```bash
# cppcheck만 실행
cppcheck --enable=warning,style --std=c11 \
    --project=build/compile_commands.json \
    --suppress=missingIncludeSystem --quiet

# clang-tidy (macOS SDK 경로 필요)
$(brew --prefix llvm)/bin/clang-tidy \
    -p build/compile_commands.json \
    --extra-arg="-isysroot$(xcrun --show-sdk-path)" \
    socket-comm/src/comm_node.c

# ASan/UBSan Debug 빌드 테스트
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
./build/debug/rtos-scheduler/test_rtos
./build/debug/packet-analyzer/test_parser
./build/debug/hal/test_hal_loopback
```

---

## 8. Docker 멀티 노드 시뮬레이션

### 이미지 빌드

```bash
docker build -t sentinel-stack ./docker/
```

### 단일 컨테이너 실행

```bash
docker run --rm \
    --cap-add=NET_ADMIN \
    --cap-add=NET_RAW \
    sentinel-stack
```

### docker-compose 멀티 노드

3개 컨테이너(commander / sensor / analyzer)가 `172.20.0.0/24` 가상 네트워크에서 통신합니다.

```bash
docker-compose -f docker/docker-compose.yml up

# 백그라운드 실행
docker-compose -f docker/docker-compose.yml up -d

# 로그 확인
docker-compose -f docker/docker-compose.yml logs -f

# 종료
docker-compose -f docker/docker-compose.yml down
```

컨테이너 역할:

| 컨테이너 | IP | 역할 |
|----------|----|------|
| sentinel-rtos | - | RTOS 시뮬레이터 독립 실행 |
| sentinel-commander | 172.20.0.10 | TCP:9000 서버, UDP:9001 발행 |
| sentinel-sensor | 172.20.0.11 | 172.20.0.10:9000 연결 |
| sentinel-analyzer | - | eth0 패킷 캡처, /output/report.csv 저장 |

---

## 9. QEMU ARM 에뮬레이션

ARM Cortex-M3 (mps2-an385 보드)에서 RTOS 스케줄러를 실행합니다.

### 사전 준비

```bash
# macOS
brew install qemu
brew install --cask gcc-arm-embedded   # arm-none-eabi-gcc

# Linux
sudo apt-get install qemu-system-arm gcc-arm-none-eabi
```

### 크로스 컴파일

```bash
cmake -B build-arm -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_FOR_QEMU=ON
cmake --build build-arm
```

> Layer 2(소켓), Layer 3(pcap), Layer 4(HAL), Layer 5(HIL)는 QEMU 빌드에서 자동 제외됩니다.

### QEMU 실행

```bash
./qemu/run_arm.sh
```

종료: `Ctrl-A` → `X`

### GDB 디버그 모드

```bash
# 터미널 1: QEMU GDB 서버 시작 (포트 1234에서 대기)
./qemu/run_arm.sh --debug

# 터미널 2: GDB 연결
arm-none-eabi-gdb build-arm/rtos-scheduler/rtos_qemu.elf \
    -ex "target remote :1234" \
    -ex "load" \
    -ex "continue"
```

---

## 빠른 참조

```bash
# 빌드
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release && cmake --build build

# 전체 테스트
ctest --test-dir build --output-on-failure

# 정적 분석
./scripts/run_analysis.sh

# 통합 데모 (tmux)
./demo/run_all.sh

# HIL 데모 (Ctrl+C로 종료)
./build/hil-simulator/hil_demo

# Docker 멀티 노드
docker-compose -f docker/docker-compose.yml up

# QEMU ARM
./qemu/run_arm.sh
```
