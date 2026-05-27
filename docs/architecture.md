# sentinel-stack 전체 시스템 설계 문서

> 버전: 1.0 | 작성일: 2025 | 작성자: Park Sang Min

---

## 1. 설계 목적

방산 임베디드 시스템에서 요구하는 핵심 역량인  
**실시간 스케줄링 → 신뢰 통신 → 트래픽 분석** 파이프라인을  
단일 통합 시스템으로 구현하여 설계·구현·검증 역량을 입증합니다.

---

## 2. 시스템 아키텍처

<img width="669" height="704" alt="Screenshot 2026-05-27 at 3 07 36 PM" src="https://github.com/user-attachments/assets/a59bda0b-892a-41c8-921d-1b25fbe54a1c" />



---

## 3. 레이어 간 연동

| 레이어 | 역할 | 연동 방향 |
|--------|------|-----------|
| Layer 1 (RTOS) | 통신 태스크 스케줄링 | 하위 → 상위 |
| Layer 2 (Comm) | 패킷 생성 및 전송 | Layer 1 태스크로 실행 |
| Layer 3 (PCAP) | 전송된 패킷 캡처·분석 | 수동 모니터링 |

---

## 4. 데이터 흐름

```
[RTOS 태스크 활성화]
      ↓
[comm_node_publish()]        ← Layer 2 발행
      ↓
[AES-256-GCM 암호화]
      ↓
[sentinel 패킷 직렬화]       ← protocol.c
      ↓
[TCP/UDP 전송]               ← tcp_transport.c / udp_transport.c
      ↓ (네트워크)
[libpcap 캡처]               ← Layer 3 수신
      ↓
[parse_packet()]             ← parser_*.c
      ↓
[rule_engine_evaluate()]     ← rule_engine.c
      ↓
[reporter_update()]          ← reporter.c (CSV + TUI)
```

---

## 5. 참고 표준 적용 내역

| 표준 | 적용 방식 |
|------|-----------|
| **MIL-STD-1553** | TCP(신뢰) + UDP(고속) 이중화 채널 구조 참고 |
| **ARINC 653** | READY/RUNNING/BLOCKED/SUSPENDED/DEAD 파티션 상태 모델 참고 |
| **MISRA-C 2012** | malloc 금지, 정적 풀, NULL 검사, 범위 검사, assert() 방어 프로그래밍 |
| **DO-178C** | 단위 테스트(assert 기반) + 지터 측정으로 소프트웨어 검증 접근 |
| **Link-16** | 메시지 타입(DATA/STATUS/CMD/HB/ACK), 토픽 기반 라우팅 |

---

## 6. 메모리 모델

MISRA-C 2012 Rule 21.3: 동적 메모리 할당 금지

```
정적 메모리 배치 (예시, ARM Cortex-M3):
  ┌─────────────────┐  ← RAM 시작 (0x20000000)
  │    .data        │  초기화된 전역 변수
  ├─────────────────┤
  │    .bss         │  미초기화 전역 변수
  ├─────────────────┤
  │  Task Pool      │  rtos_task_t[MAX_TASKS=32]     ≈ 5KB
  ├─────────────────┤
  │  Packet Pool    │  sentinel_packet_t 버퍼        ≈ 4KB
  ├─────────────────┤
  │  PubSub Pool    │  pubsub_topic_t[16]            ≈ 2KB
  ├─────────────────┤
  │    Stack        │
  └─────────────────┘  ← RAM 끝
```

동적 할당이 필요한 외부 라이브러리(libpcap, OpenSSL) 사용 시  
초기화 단계에서만 허용하고 실시간 경로에서는 호출 금지.

---

## 7. 플랫폼 이식성

| 기능 | macOS | Linux |
|------|-------|-------|
| I/O 멀티플렉싱 | kqueue (io_kqueue.c) | epoll (io_epoll.c) |
| 고해상도 타이머 | CLOCK_MONOTONIC | CLOCK_MONOTONIC |
| 패킷 캡처 | libpcap (기본 내장) | libpcap-dev |
| 암호화 | OpenSSL 3.x | OpenSSL 3.x |
| 크로스 컴파일 | arm-none-eabi-gcc | arm-none-eabi-gcc |
