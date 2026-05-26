# Layer 3: 패킷 분석기 설계 명세

> 버전: 1.0 | 참고: libpcap, Snort IDS

---

## 1. 개요

libpcap 기반 실시간 네트워크 패킷 캡처 및 분석 도구입니다.
sentinel Layer 2 통신 트래픽을 수동적으로 모니터링하고 이상을 탐지합니다.

---

## 2. 파서 계층 구조

```
Raw Bytes
    ↓
[parser_eth.c]      Ethernet II 프레임 파싱
    ↓ (EtherType=0x0800)
[parser_ip.c]       IPv4 헤더 파싱
    ↓ (proto=6/17)
[parser_tcp.c]      TCP 헤더 파싱
[parser_ip.c]       UDP 헤더 파싱
    ↓ (port 9000/9001 AND magic=0x53454E54)
[parser_custom.c]   sentinel 프로토콜 파싱 + CRC 검증
    ↓
parsed_packet_t     통합 파싱 결과
```

---

## 3. 탐지 룰 엔진

Snort IDS 문법을 참고한 룰 기반 탐지 엔진입니다.

### 기본 탐지 룰

| 규칙 ID | 탐지 조건 | 임계치 | 윈도우 | 설명 |
|---------|-----------|--------|--------|------|
| 1001 | SYN Flood | 20회 | 1초 | DoS 공격 탐지 |
| 1002 | Port Scan | 15회 | 5초 | 포트 스캔 탐지 |
| 1003 | High PPS | 10,000pps | 1초 | 이상 트래픽량 |
| 2001 | Retransmit Storm | 10회 | 5초 | sentinel 재전송 폭주 |
| 2002 | Invalid Magic | 1회 | 즉시 | 프로토콜 위반 |
| 2003 | Unauthorized Node | 1회 | 즉시 | 비인가 노드 ID |

### 슬라이딩 윈도우

```
시간 →  [초1] [초2] [초3] [초4] [초5]
버킷 →   N1    N2    N3    N4    N5
                                  ↑ 현재
윈도우 합계 = Σ(window_sec 내 버킷값)
```

---

## 4. 출력 형식

### 실시간 터미널 출력

```
[  0.001s] ETH | IP 127.0.0.1 → 127.0.0.1 | TCP 52341 → 9000
             └─ SENTINEL v1 | NODE:1→2 | TOPIC:STATUS | len=128 | CRC:OK
[  0.002s] ETH | IP 127.0.0.1 → 127.0.0.1 | UDP 52342 → 9001
             └─ SENTINEL v1 | NODE:1→255 | TOPIC:SENSOR_DATA | len=64 | MULTICAST
```

### CSV 리포트

```csv
timestamp,src_ip,dst_ip,src_port,dst_port,proto,len,sentinel,topic,anomaly
1735689600,127.0.0.1,127.0.0.1,52341,9000,TCP,180,YES,STATUS,0
```

### 10초 집계 요약

```
=== 10s 집계 ===
총 패킷    : 48,291
Sentinel   : 47,103 (97.5%)
이상 탐지  : 2
평균 크기  : 98.3 bytes
================
```
