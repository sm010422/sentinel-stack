# sentinel-stack 포트폴리오 활용 가이드 & 예상 면접 질문

> 방산 임베디드 시스템 직무 지원 시 이 프로젝트를 어떻게 설명하고,
> 어떤 질문이 나올 수 있는지 정리한 문서입니다.

---

## 1. 이 프로젝트의 포트폴리오 강점

### 1.1 차별화 포인트 요약

| 포인트 | 일반 포트폴리오 | sentinel-stack |
|--------|----------------|----------------|
| RTOS | "스레드로 태스크 만들었어요" | RMS/EDF 이론(Liu & Layland) 기반, 스케줄 가능성 수식 검증 |
| 통신 | "소켓 프로그래밍 했어요" | 커스텀 바이너리 프로토콜 + AES-256-GCM + TCP/UDP 이중화 |
| 보안 | 없거나 bcrypt 정도 | 매 패킷 IV 재생성, GCM 인증 태그, IV 재사용 위험 인지 |
| 메모리 | malloc 자유롭게 사용 | 정적 풀만 사용, MISRA-C Rule 21.3 준수 |
| 검증 | "동작합니다" | 단위 테스트 5종 + 지터 측정 + 벤치마크 |
| 참고 표준 | 없음 | MIL-STD-1553, ARINC 653, MISRA-C, DO-178C, Link-16 |
| 플랫폼 | 단일 OS | macOS/Linux + QEMU ARM Cortex-M3 크로스 컴파일 |

### 1.2 자기소개서/지원서에 쓸 수 있는 표현

아래 표현들은 실제 코드와 문서로 뒷받침되므로 근거 있게 쓸 수 있습니다.

```
"RMS, EDF 알고리즘을 Liu & Layland 이론에 근거하여 구현하고,
CPU 이용률 한계(n·(2^(1/n)-1))로 스케줄 가능성을 수식 검증했습니다."

"AES-256-GCM 인증 암호화로 기밀성과 무결성을 동시에 보장하고,
매 패킷마다 /dev/urandom 기반 IV를 재생성하여 IV 재사용 취약점을 방어했습니다."

"MISRA-C 2012 Rule 21.3을 준수하여 실시간 경로에서 동적 메모리 할당을 금지하고,
정적 메모리 풀 방식으로 결정론적 실행을 보장했습니다."

"MIL-STD-1553의 이중 버스 개념을 참고하여 TCP(신뢰) + UDP(고속) 이중화 채널을 설계했습니다."

"epoll(Linux) / kqueue(macOS) 추상화 레이어를 구현하여 동일한 통신 코드가
플랫폼 무관하게 동작하도록 이식성을 확보했습니다."
```

---

## 2. 예상 면접 질문 & 답변 가이드

### 2.1 RTOS / 스케줄링

---

**Q1. RMS와 EDF의 차이를 설명하고, 각각 언제 유리한가요?**

```
RMS (Rate Monotonic Scheduling):
  - 정적 우선순위: 주기가 짧을수록 높은 우선순위 (설계 시 고정)
  - 스케줄 가능성 한계: U ≤ n × (2^(1/n) - 1)  → n→∞ 시 ln(2) ≈ 0.693
  - 장점: 런타임 오버헤드 거의 없음, 분석이 단순, 결정론적
  - 단점: CPU 활용률 최대 ~69%까지만 보장
  - 방산 적합 상황: 태스크 구성이 사전에 고정된 항법/제어 시스템

EDF (Earliest Deadline First):
  - 동적 우선순위: 남은 데드라인이 가장 짧은 태스크 선택
  - 스케줄 가능성 한계: U ≤ 1.0 (이론적 최적)
  - 장점: CPU 활용률 극대화
  - 단점: 과부하 상황에서 어떤 태스크가 실패할지 예측 어려움
  - 방산 적합 상황: 비정기 긴급 명령 처리가 필요한 경우
```

이 프로젝트에서: `rms.c`가 period 기준 버블 정렬 → 우선순위 배정,
`edf.c`가 활성화 시점에 `abs_deadline_ms = now + period_ms` 계산 후 최솟값 선택

---

**Q2. WCET(Worst Case Execution Time)가 왜 중요한가요?**

```
스케줄 가능성 분석의 핵심 입력값입니다.

U = Σ(WCET_i / T_i) ≤ 한계

WCET를 과소 추정하면 → 실제 실행이 데드라인을 넘어도 분석상 "스케줄 가능"으로 나옴
WCET를 과대 추정하면 → 실제로 스케줄 가능한데도 태스크를 추가하지 못함

방산에서는 정적 분석 도구(AbsInt aiT, Bound-T 등)로 WCET를 증명합니다.
이 프로젝트에서는 timer_sleep_us()로 WCET를 시뮬레이션하고 지터를 측정했습니다.
```

---

**Q3. 우선순위 역전(Priority Inversion) 문제를 어떻게 다뤘나요?**

```
우선순위 역전: 낮은 우선순위 태스크가 Mutex를 잡고 있어서
높은 우선순위 태스크가 대기하는 현상. Mars Pathfinder 사고 원인.

해결책:
  - Priority Inheritance: Mutex 보유 중인 태스크를 대기자의 우선순위로 임시 승격
  - Priority Ceiling: Mutex에 최고 우선순위를 미리 등록, 취득 즉시 그 우선순위로 승격

이 프로젝트에서: rtos_mutex_t 설계 시 Priority Ceiling 프로토콜을 구조체에 명시(ceiling 필드)했습니다.
POSIX 시뮬레이션이므로 실제 커널 스케줄러 개입 없이 소프트웨어 수준에서 처리합니다.
```

---

**Q4. POSIX 기반 시뮬레이션의 한계는 무엇인가요? 실제 RTOS와 어떻게 다른가요?**

이 질문은 면접관이 지원자의 깊이를 보는 질문입니다. 솔직하게 답하는 것이 좋습니다.

```
이 프로젝트의 한계:

1. 선점 보장 없음
   - 실제 RTOS: 높은 우선순위 태스크가 낮은 우선순위를 즉시 선점
   - 이 구현: nanosleep() 기반 협조적 틱, OS 스케줄러의 개입을 받음

2. 타이머 해상도
   - 실제 RTOS: SysTick 인터럽트 기반 1µs 이하 분해능
   - 이 구현: clock_gettime(CLOCK_MONOTONIC)이지만 OS 스케줄링 지터의 영향을 받음

3. 인터럽트 처리
   - 실제 RTOS: IRQ 벡터 테이블, 인터럽트 우선순위(NVIC)
   - 이 구현: 없음 (QEMU 포팅 시 링커 스크립트로 벡터 테이블만 정의)

4. 스택 분리
   - 실제 RTOS: 태스크마다 독립 스택
   - 이 구현: 단일 프로세스 내 함수 호출로 시뮬레이션

보완 방향: FreeRTOS 포팅 또는 Zephyr RTOS 위에서 동일 알고리즘 동작 확인
```

---

### 2.2 통신 / 프로토콜 / 보안

---

**Q5. AES-256-GCM을 선택한 이유는? CBC 모드와 비교해서 설명해보세요.**

```
AES-256-CBC의 한계:
  - 기밀성(Confidentiality)만 제공
  - 변조 감지 없음 → 공격자가 암호문을 조작해도 복호화 성공
  - 패딩 오라클 공격에 취약

AES-256-GCM (Galois/Counter Mode):
  - 인증 암호화(AEAD: Authenticated Encryption with Associated Data)
  - 기밀성 + 무결성 + 인증 동시 제공
  - 16바이트 인증 태그: 1비트라도 조작되면 복호화 실패
  - 하드웨어 가속(AES-NI) 지원으로 성능 우수

전술 통신에서는 변조된 명령이 무기 시스템에 도달하면 치명적이므로
무결성 보장 없는 CBC 모드는 적합하지 않습니다.
```

코드 위치: `socket-comm/src/crypto.c` — `EVP_aes_256_gcm()` 사용

---

**Q6. IV(Initialization Vector)를 왜 매 패킷마다 새로 생성하나요?**

```
GCM 모드에서 동일한 (Key, IV) 쌍이 두 번 사용되면:

  - 두 암호문의 XOR → 두 평문의 XOR 노출
  - 인증 키 복구 가능 → 태그 위조 가능 → 완전한 보안 붕괴

따라서 매 패킷마다 RAND_bytes()로 12바이트 무작위 IV를 생성하고
헤더에 평문으로 포함시킵니다 (IV는 기밀이 아님).

이 방식의 단점: 패킷마다 RAND_bytes() 호출 오버헤드
대안: 카운터 기반 IV (시퀀스 번호를 IV로) — 단, 동기화 필요
```

코드 위치: `socket-comm/src/crypto.c` — `crypto_generate_iv()`

---

**Q7. TCP와 UDP를 동시에 쓴 이유는? 실제 방산 시스템에서의 근거는?**

```
MIL-STD-1553 이중 버스(Primary / Redundant Bus) 개념 참고:

TCP 채널 (Port 9000):
  - 신뢰 채널: 3-way handshake, 재전송, 순서 보장
  - 용도: CMD(명령), STATUS(상태) — 손실 허용 불가
  - 단점: 재전송으로 인한 지연 발생 가능

UDP 채널 (Port 9001):
  - 고속 채널: 연결 없음, 재전송 없음
  - 용도: SENSOR_DATA, HEARTBEAT — 최신 데이터가 중요, 손실 허용
  - 단점: 전달 보장 없음

이 분리가 중요한 이유: 네트워크 혼잡 시 TCP 재전송이 UDP 실시간 데이터에
영향을 주지 않도록 채널을 분리합니다.
```

---

**Q8. CRC-32와 GCM 인증 태그를 둘 다 쓰는 이유는?**

```
역할이 다릅니다:

GCM 인증 태그 (16 bytes):
  - 보안 목적: 악의적 변조 감지
  - 키를 알아야만 생성/검증 가능

CRC-32 (4 bytes):
  - 신뢰성 목적: 전송 오류(비트 플립, 잡음) 감지
  - 키 없이 계산 가능

실제로 GCM 태그 검증 전에 CRC가 틀리면 빠른 조기 폐기(early rejection)가 가능합니다.
비유: CRC는 봉투의 찢김 확인, GCM 태그는 서명 위조 확인.
```

---

### 2.3 패킷 분석 / 이상 탐지

---

**Q9. SYN Flood 탐지 로직을 설명해보세요.**

```
슬라이딩 윈도우 카운터:

  - 8개 버킷, 각 1초 단위 (총 8초 윈도우)
  - 매 초 새 버킷에 카운트, 8초 이전 버킷 초기화
  - 현재 8초간 SYN 패킷 수 합산

  임계값 초과 시 경보:
    SID 1001: 10초 내 SYN 100개 초과 → SYN Flood 의심

장점: O(1) 업데이트, 고정 메모리(정적 배열)
단점: 버킷 경계에서 최대 2배 오차 가능 (토큰 버킷 방식으로 개선 가능)
```

코드 위치: `packet-analyzer/src/rule_engine.c` — `window_add()`

---

**Q10. libpcap을 무차별 모드(promiscuous mode)로 쓸 때의 보안 고려사항은?**

```
무차별 모드(promiscuous mode):
  - NIC가 자신의 MAC 주소와 다른 패킷도 수신
  - 네트워크 모니터링 필수 조건
  - 요구 권한: Linux CAP_NET_RAW 또는 root

보안 위험:
  - 해당 프로세스가 탈취되면 네트워크 전체 도청 가능
  - 따라서 sentinel_pcap은 최소 권한으로 실행 권장

이 프로젝트에서:
  - Docker에서 cap_add: [NET_ADMIN, NET_RAW]로 최소 권한만 부여
  - 실제 환경에서는 전용 분석 노드를 분리 운용
```

---

### 2.4 표준 / 방산 지식

---

**Q11. MISRA-C 2012에서 동적 메모리를 금지하는 이유는?**

```
세 가지 이유:

1. 결정론적 실행 (Determinism)
   malloc()은 실행마다 다른 시간이 걸릴 수 있음
   실시간 시스템에서 WCET 분석 불가능

2. 힙 단편화 (Heap Fragmentation)
   장시간 실행 시 힙이 단편화되어 할당 실패 가능
   임무 중 메모리 부족 = 시스템 실패

3. 분석 불가능성 (Unanalyzability)
   malloc 실패 경로를 테스트로 완전히 커버하기 어려움

대안: 사전에 크기가 알려진 정적 배열/풀 사용
  ex) rtos_task_t tasks[MAX_TASKS] = {0};
```

---

**Q12. DO-178C Level 분류에 대해 설명해보세요.**

```
항공 소프트웨어 안전성 표준 (DAL: Design Assurance Level):

  Level A (Catastrophic): 실패 시 추락 → 최고 수준 검증
  Level B (Hazardous):    심각한 부상
  Level C (Major):        승객 불편, 부하 증가
  Level D (Minor):        약간의 불편
  Level E (No Effect):    안전에 영향 없음

Level이 높을수록:
  - 코드 커버리지 요건 상향 (Level A: MC/DC 커버리지 100%)
  - 독립적 검증(IV&V) 필요
  - 요구사항 추적성(traceability) 문서화

이 프로젝트의 포지션:
  DO-178C를 직접 인증받은 것은 아니지만, 설계 접근법(단위 테스트, 지터 측정,
  정적 메모리, WCET 분석)은 Level C 수준의 검증 문화를 참고했습니다.
```

---

**Q13. MIL-STD-1553 버스의 이중화가 왜 필요한가요?**

```
MIL-STD-1553: 1553년대 군용 직렬 통신 버스 표준 (1MHz, 차동 신호)

이중화 이유:
  1. 단일 버스 단선 시 전투기/미사일 시스템 전체 통신 마비
  2. A버스(Primary) + B버스(Redundant) → 하나 실패해도 나머지로 운용 계속
  3. EMI(전자기 간섭)가 심한 전장 환경에서 한 버스 간섭 받아도 다른 버스 정상

sentinel-stack에서의 참고:
  TCP(신뢰, 재전송) + UDP(고속, 실시간) 이중 채널 구조가 이 개념을 소프트웨어로 모방
```

---

### 2.5 시스템 설계 심화

---

**Q14. epoll과 kqueue의 차이는?**

```
두 방식 모두 I/O 이벤트 통지 메커니즘 (select/poll의 개선)

epoll (Linux):
  epoll_create1() → fd 하나 생성
  epoll_ctl(EPOLL_CTL_ADD) → fd 등록
  epoll_wait() → 이벤트 수신 (O(1), 등록 fd 수와 무관)

kqueue (BSD/macOS):
  kqueue() → 큐 생성
  kevent(EV_ADD) → 이벤트 등록
  kevent(timeout) → 이벤트 수신
  차이: 파일, 소켓, 프로세스, 신호 등 다양한 이벤트 통합 처리 가능

select의 한계:
  FD_SET 비트마스크 → 최대 FD_SETSIZE(1024)개 제한
  매번 전체 fd 세트 재스캔 → O(n)

코드 위치:
  socket-comm/src/io_epoll.c  (Linux)
  socket-comm/src/io_kqueue.c (macOS)
  CMake에서 플랫폼에 따라 자동 선택
```

---

**Q15. 이 시스템의 단일 장애점(SPOF, Single Point of Failure)은 어디인가요?**

면접관이 설계 사고를 보는 질문입니다.

```
현재 구조에서의 SPOF:

1. Commander 노드 (172.20.0.10)
   - 모든 Sensor가 Commander에 연결
   - Commander 장애 시 전체 통신 두절
   - 대안: Commander 이중화 (Active-Standby), Raft/Paxos 합의

2. 암호화 키 관리
   - 현재 AES 키가 comm_node.c에 하드코딩
   - 실제 환경: HSM(Hardware Security Module) 또는 키 교환 프로토콜(ECDH) 필요

3. 패킷 분석기 단일 인터페이스
   - NIC 장애 시 모니터링 불가
   - 대안: 미러 포트(SPAN) 또는 TAP 장치 이중화
```

---

**Q16. 실제 방산 하드웨어(FPGA, DSP, VPX 보드)에 이 코드를 포팅한다면 무엇이 달라지나요?**

```
1. OS 변경
   현재: POSIX(macOS/Linux)
   실제: VxWorks, Integrity RTOS, LynxOS (DO-178C 인증 RTOS)

2. 드라이버
   현재: Berkeley 소켓 API
   실제: MIL-STD-1553 버스 드라이버, ARINC 664(AFDX) 스택

3. 타이머
   현재: clock_gettime(CLOCK_MONOTONIC)
   실제: 하드웨어 타이머(SysTick, 외부 FPGA 클럭)에서 직접 인터럽트

4. 패킷 캡처
   현재: libpcap (OS 커널 바이패스 없음)
   실제: DPDK 또는 전용 네트워크 카드의 하드웨어 타임스탬프

5. 암호화
   현재: OpenSSL (소프트웨어 구현)
   실제: 전용 암호화 칩(TPM, NSA Suite B 하드웨어)

이 프로젝트의 코드는 하드웨어 추상화 레이어(timer.h, io_multiplexer.h) 구조가
되어 있어서 실제 드라이버로 교체 시 상위 레이어 수정이 최소화됩니다.
```

---

## 3. 면접 전 직접 해보면 좋은 것들

### 3.1 실제 실행으로 숫자 파악하기

면접에서 "실제로 얼마나 나왔어요?" 라는 질문에 답하려면
직접 돌려보고 숫자를 외워두세요.

```bash
# RTOS 지터 실측 (중요!)
./build/rtos-scheduler/benchmark_jitter 2>&1 | tee jitter_actual.txt

# 단위 테스트 전체 통과 확인
ctest --test-dir build --output-on-failure

# RTOS 시뮬레이터 실행해보기
./build/rtos-scheduler/rtos_sim --algo rms --tasks 5 --duration 10s

# EDF vs RMS 비교
./build/rtos-scheduler/rtos_sim --algo edf --tasks 5 --duration 10s
```

### 3.2 코드를 보여줄 때 강조할 파일들

```
1. rtos-scheduler/src/rms.c
   → Liu & Layland 수식이 코드에 직접 반영된 부분 설명

2. socket-comm/src/crypto.c
   → EVP_CIPHER_CTX_new/free 패턴, IV 생성 코드

3. packet-analyzer/src/rule_engine.c
   → 슬라이딩 윈도우 카운터, window_add() 함수

4. socket-comm/include/protocol.h
   → packed 구조체, 빅엔디안 직렬화 이유 설명

5. rtos-scheduler/include/task.h
   → RTOS_ASSERT 매크로, 정적 배열, MISRA-C 준수 구조
```

### 3.3 약점 인정 준비

면접관이 한계를 물으면 솔직하게 + 개선 방향으로 마무리하세요.

```
"이 프로젝트는 실제 RTOS가 아닌 POSIX 시뮬레이션이어서
커널 레벨 선점이 보장되지 않습니다.
FreeRTOS나 Zephyr 위에서 동일한 알고리즘을 돌리는 것이 다음 목표입니다."

"AES 키가 현재 하드코딩되어 있어서 실제 환경에서는
HSM이나 ECDH 키 교환 프로토콜 연동이 필요합니다."
```

---

## 4. 직무별 어필 포인트 차이

### 임베디드 SW 개발 직무

- Layer 1 (RTOS) 중점 설명
- MISRA-C 정적 메모리, 지터 측정, WCET 분석
- QEMU ARM Cortex-M3 크로스 컴파일 과정
- 링커 스크립트(linker.ld) — 플래시/RAM 메모리 맵 이해

### 통신/미들웨어 개발 직무

- Layer 2 (Socket Comm) 중점 설명
- 바이너리 프로토콜 설계 (헤더 구조, 직렬화, CRC)
- TCP/UDP 이중화, epoll/kqueue I/O 멀티플렉싱
- Publisher-Subscriber 패턴 (DDS 개념)

### 사이버보안 / 시스템 보안 직무

- AES-256-GCM 선택 근거, IV 관리
- 패킷 분석기 + 룰 엔진 (SYN Flood 탐지 로직)
- Docker CAP_NET_RAW 최소 권한 설계
- 이상 탐지 슬라이딩 윈도우 알고리즘

### SW 검증 / QA 직무

- 5개 단위 테스트 (ctest), 테스트 설계 방법
- 지터 측정 방법론, 목표 대비 달성 표
- DO-178C 검증 접근법 참고 내용
- 정적 분석 가능한 구조 (no malloc, no VLA)

---

## 5. 이 프로젝트 하나로 이야기할 수 있는 CS 개념 목록

면접에서 이 프로젝트를 계기로 물어볼 수 있는 CS 기초 질문들입니다.
모두 이 코드에서 직접 나온 개념이므로 코드와 연결지어 답할 수 있습니다.

| CS 개념 | 연결 코드 |
|---------|-----------|
| 선점형 스케줄링 vs 협조적 스케줄링 | scheduler.c 시뮬레이션 방식 |
| 뮤텍스 vs 세마포어 차이 | task.h 동기화 객체 설계 |
| 빅엔디안 vs 리틀엔디안 | protocol.c htonl/htons 사용 이유 |
| 스택 vs 힙 메모리 | MISRA-C 정적 배열 선택 이유 |
| 소켓 논블로킹 I/O | io_kqueue.c / io_epoll.c |
| TCP 3-way handshake | tcp_transport.c accept/connect |
| 암호화 모드 (CBC vs GCM) | crypto.c EVP API |
| CRC 오류 검출 원리 | protocol.c CRC-32 룩업 테이블 |
| 슬라이딩 윈도우 | rule_engine.c window_add() |
| 인터럽트 벡터 테이블 | qemu/linker.ld .isr_vector |
| 공유 라이브러리 vs 정적 라이브러리 | CMake STATIC 라이브러리 구성 |
| 교차 컴파일(Cross Compilation) | qemu/run_arm.sh, arm-none-eabi-gcc |
