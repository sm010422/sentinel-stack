# 참고 표준 및 문헌

---

## 군사·방산 표준

| 표준 | 설명 | 적용 영역 |
|------|------|-----------|
| **MIL-STD-1553B** | Digital Time Division Command/Response Multiplex Data Bus | Layer 2 이중화 채널 설계 참고 |
| **ARINC 653** | Avionics Application Software Standard Interface | Layer 1 파티션 스케줄러 상태 모델 |
| **MISRA-C 2012** | Guidelines for the Use of the C Language in Critical Systems | 전체 코딩 컨벤션 |
| **DO-178C** | Software Considerations in Airborne Systems and Equipment Certification | 소프트웨어 검증 접근법 |
| **Link-16** | 전술 데이터링크 메시지 포맷 개념 (분류 해제 자료) | 메시지 타입 및 토픽 설계 참고 |

---

## 학술 논문

| 논문 | 내용 | 적용 |
|------|------|------|
| Liu & Layland (1973) "Scheduling Algorithms for Multiprogramming in Hard Real-Time Environments" | RMS 이론 및 이용률 한계 U_bound = n(2^(1/n)−1) | rms.c 알고리즘 |
| Buttazzo (2011) "Hard Real-Time Computing Systems" | EDF 최적성 증명, 스케줄 가능성 분석 | edf.c 알고리즘 |

---

## 오픈소스 및 공식 문서

| 자료 | URL | 적용 |
|------|-----|------|
| FreeRTOS Kernel | https://github.com/FreeRTOS/FreeRTOS-Kernel | RTOS 구조 참고 |
| libpcap 문서 | https://www.tcpdump.org/manpages/pcap.3pcap.html | Layer 3 캡처 |
| OpenSSL EVP API | https://www.openssl.org/docs/man3.0/ | AES-256-GCM 구현 |
| Snort IDS | https://www.snort.org/documents | 룰 문법 참고 |
| OMG DDS 표준 | https://www.dds-foundation.org/ | PubSub 설계 참고 |

---

## NIST 보안 문서

| 문서 | 내용 |
|------|------|
| NIST SP 800-38D | GCM(Galois/Counter Mode) 표준 — AES-GCM 128비트 태그 |
| NIST FIPS 197 | AES 표준 — 256비트 키 강도 |
