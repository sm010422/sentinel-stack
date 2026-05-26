# Layer 1: RTOS 스케줄러 설계 명세

> 버전: 1.0 | 참고: ARINC 653, Liu & Layland 1973

---

## 1. 설계 목표

| 요건 | 목표값 |
|------|--------|
| 타이머 지터 (평균) | < 5 µs |
| 타이머 지터 (최대) | < 50 µs |
| 컨텍스트 스위치 오버헤드 | < 5 µs |
| 데드라인 미스율 (5태스크, RMS U<0.693) | 0% |

---

## 2. 알고리즘

### 2.1 Rate Monotonic Scheduling (RMS)

**이론적 근거**: Liu & Layland (1973), ACM

- 주기가 짧을수록 높은 우선순위 (정적 배정)
- 스케줄 가능성 조건:

```
U = Σ(WCET_i / T_i) ≤ n × (2^(1/n) - 1)
```

- n → ∞ 극한: U_bound → ln(2) ≈ 0.6931

**구현**: `rms.c`
- `rms_assign_priorities()`: 버블 정렬로 period 기준 우선순위 배정
- `rms_select_next()`: 최고 우선순위 READY 태스크 선택
- `rms_utilization()`: CPU 사용률 계산

### 2.2 Earliest Deadline First (EDF)

- 동적 우선순위: 절대 데드라인이 가장 가까운 태스크 선택
- 단일 프로세서 최적: U ≤ 1.0 이면 스케줄 보장
- 비정기 긴급 태스크에 유연하게 대응

**구현**: `edf.c`
- `edf_update_deadlines()`: 태스크 활성화 시 절대 데드라인 계산
- `edf_select_next()`: 최소 abs_deadline_ms READY 태스크 선택

---

## 3. 태스크 모델

```c
// 실시간 태스크 파라미터 관계 (Implicit Deadline 기준)
WCET_us ≤ deadline_ms × 1000 ≤ period_ms × 1000
```

### 방산 태스크 예시

| 태스크명 | 주기 | 데드라인 | WCET | 우선순위(RMS) | 역할 |
|---------|------|---------|------|-------------|------|
| BUS_MGR | 2ms | 2ms | 300µs | 0 (최고) | MIL-STD-1553 버스 관리 |
| SENSOR | 5ms | 5ms | 800µs | 1 | 센서 데이터 수집 |
| NAV | 10ms | 10ms | 1.5ms | 2 | 항법 계산 |
| CTRL | 20ms | 20ms | 3ms | 3 | 제어 루프 |
| STATUS | 50ms | 50ms | 8ms | 4 | 상태 보고 |

RMS 사용률 U = 0.15+0.16+0.15+0.15+0.16 = **0.77** (n=5 한계 0.74 초과 가능성)

---

## 4. 지터 측정

```
지터 = 실제 활성화 시각(µs) - 예상 활성화 시각(µs)
예상 = n × period_ms × 1000  (n번째 활성화)
```

측정 도구: `test_jitter.c`
- nanosleep() 자체 지터 측정 (하한선)
- 스케줄러 통합 지터 측정

---

## 5. 동기화 객체

| 객체 | 구현 | 용도 |
|------|------|------|
| Mutex | 정적 배열, Priority Ceiling | 공유 자원 보호 |
| Semaphore | 계수 세마포어, 정적 배열 | 이벤트 신호, 자원 제한 |
| Message Queue | 링버퍼, 정적 배열 | 태스크 간 데이터 전달 |

---

## 6. QEMU ARM 포팅

타겟: ARM Cortex-M3 (mps2-an385 보드)
- SysTick 타이머 → 1ms tick
- PendSV 인터럽트 → 컨텍스트 스위치
- POSIX 시뮬레이션 코드와 동일한 스케줄러 코어 사용
