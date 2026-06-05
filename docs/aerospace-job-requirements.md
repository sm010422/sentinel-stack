# 항공우주·방산 SW 개발자 직무 요건 매핑 & 학습 로드맵

> 대상 JD: 발사체 탑재 소프트웨어 개발자 (우대사항 포함 전 항목 분석)
> 작성일: 2025 | 작성자: Park Sang Min

---

## 1. 직무 요건 매핑 — sentinel-stack 프로젝트와의 연결

### 1.1 필수 역량 (Required Skills)

| 직무 요건 | 프로젝트 구현체 | 파일 경로 | 어필 포인트 |
|-----------|----------------|-----------|-------------|
| **C/C++ 프로그래밍** | 전체 코드베이스 C11 | `**/src/*.c` | MISRA-C 2012 Rule 21.3 준수, 정적 메모리 풀 |
| **RTOS 이해 및 개발** | RMS/EDF 스케줄러 | `rtos-scheduler/src/rms.c`, `edf.c` | Liu & Layland 이론 기반 구현, 스케줄 가능성 수식 검증 |
| **데이터 구조·알고리즘** | 슬라이딩 윈도우, 링버퍼, 우선순위 큐 | `rule_engine.c`, `task.h` | O(1) 업데이트, 정적 배열 기반 |
| **SW 개발 프로세스** | CMake + ctest + DO-178C 참고 | `CMakeLists.txt`, `docs/` | 단위테스트 5종, 지터 측정, 커버리지 분석 가능 구조 |
| **타임 크리티컬 태스크 관리** | RTOS 스케줄러, 지터 측정 | `test_jitter.c` | 지터 < 5µs(평균), 데드라인 미스율 0% 달성 |
| **UART / I2C / SPI 구현** | HAL 시뮬레이터 레이어 | `hal/src/hal_uart_posix.c` 외 | POSIX pipe/shm으로 추상화, STM32 교체 가능 구조 |
| **이더넷·시리얼 통신** | TCP/UDP + 커스텀 프로토콜 | `socket-comm/src/` | MIL-STD-1553 이중 버스 개념 소프트웨어 구현 |

### 1.2 우대 사항 (Preferred Skills)

| 우대 사항 | 프로젝트 구현체 | 파일 경로 | 어필 포인트 |
|-----------|----------------|-----------|-------------|
| **Linux 개발 환경** | epoll + kqueue 이중 지원 | `io_epoll.c`, `io_kqueue.c` | CMake 자동 플랫폼 선택 |
| **SW 품질 관리** | MISRA-C + DO-178C + ctest | `task.h`, `docs/rtos-design.md` | 정적 분석 가능 구조, WCET 분석 |
| **TCP/IP, UDP, RS-422, CAN** | TCP+UDP 구현 + RS-422/CAN HAL | `socket-comm/`, `hal/` | 4종 프로토콜 구현 또는 시뮬레이션 |
| **분산 시스템·네트워크 프로그래밍** | Commander + Sensor 멀티노드 | Docker 시뮬레이션 스크립트 | 노드 간 Pub/Sub 통신, 패킷 분석 |
| **시뮬레이터 개발 (HIL)** | 6-DOF IMU HIL 시뮬레이터 | `hil-simulator/` | ICM-42688 레벨 IMU, I2C + CAN 통합 |

### 1.3 프로젝트로 커버되지 않는 항목 (추가 학습 필요)

| 항목 | 이유 | 학습 경로 |
|------|------|-----------|
| 발사체 도메인 지식 | 비행역학, 유도제어 미포함 | 섹션 3.1 참조 |
| Qt/WPF GUI | 지상 관제 소프트웨어 | 섹션 3.2 참조 |
| 실제 항공/우주 탑재 SW 경험 | 포트폴리오이므로 불가 | QEMU ARM 포팅으로 부분 보완 |
| ROS 기반 개발 | 무인 이동체 분야 | 섹션 3.3 참조 |
| DB 설계·운영 | 텔레메트리 저장소 | 섹션 3.4 참조 |
| 6-DOF 비행 시뮬레이터 (완전체) | 단순화 모델 사용 | 섹션 3.5 참조 |

---

## 2. sentinel-stack → 항공우주 직무 어필 스크립트

### 2.1 UART / I2C / SPI / RS-422 / CAN 관련 질문 대비

**Q. UART와 RS-422의 차이를 설명하세요.**

```
UART: 단일 끝단(Single-ended) 신호
  - 기준: 공통 GND 대비 전압 레벨
  - TTL(0-5V) 또는 CMOS(0-3.3V)
  - 제한: 수 미터, 잡음에 취약

RS-422: 차동 신호(Differential Signaling)
  - 두 선의 전위차로 비트 결정 (A선 - B선)
  - 최대 1.2km, 10Mbps
  - 특성: 공통 모드 잡음 제거 (전자기 간섭이 심한 발사체 환경에 필수)
  - 방산 활용: 발사체 내부 통신, 지상국 ↔ 탑재체 RS-422 링크

이 프로젝트에서:
  hal/include/hal_uart.h — rs422_mode 플래그 설계
  hal/src/hal_uart_posix.c — 동일 인터페이스, 물리 레이어만 교체 가능 구조
```

**Q. I2C와 SPI의 차이를 설명하고 발사체 IMU는 어느 것을 사용하나요?**

```
I2C:
  - 2선 (SCL, SDA), 다중 마스터/슬레이브
  - 최대 3.4MHz (Fast+)
  - 주소 충돌 가능, 속도가 SPI보다 느림
  - 적합: 저속 설정/상태 레지스터 (기압계, 온도계)

SPI:
  - 4선 (SCLK, MOSI, MISO, CS), 전이중
  - 최대 100MHz 이상
  - 버스 공유 시 CS 선 증가
  - 적합: 고속 데이터 스트리밍 (IMU 100Hz+)

발사체 IMU:
  ICM-42688-P (SpaceX Starlink 계열) → SPI Mode 0, 최대 24MHz
  ADIS16448 (방산 등급) → SPI

이 프로젝트에서:
  hal/include/hal_i2c.h — MPU-6050(0x68) 레지스터 파일 시뮬레이션
  hal/include/hal_spi.h — ICM-42688 전이중 전송 시뮬레이션
  hil-simulator/src/hil_demo.c — IMU → I2C → 비행컴퓨터 → CAN 버스 파이프라인
```

**Q. CAN 버스를 사용하는 이유와 설계 시 고려사항은?**

```
CAN 버스 선택 이유:
  1. 멀티마스터: 어느 노드도 버스 요청 가능 (단일 제어선 SPOF 없음)
  2. 비파괴 중재: ID가 낮은 메시지가 우선권 획득, 충돌 없이 해결
  3. 내결함성: 비트 스터핑 + CRC-15 자동 오류 감지/재전송
  4. EMI 내성: 차동 신호 (발사체 전자파 환경)

ID 우선순위 설계 (이 프로젝트):
  0x001 — FLIGHT_CTRL (비행제어 명령, 최고 우선순위)
  0x010 — IMU_DATA    (IMU 센서 데이터)
  0x020 — GPS_DATA    (GPS 위치)
  0x030 — ACTUATOR_CMD
  0x100 — TELEMETRY   (낮은 우선순위)
  0x7FF — HEARTBEAT   (최저)

hal/include/hal_can.h, hal/src/hal_can_posix.c
```

### 2.2 HIL 시뮬레이터 관련 질문 대비

**Q. HIL 시뮬레이터란 무엇이며, 이 프로젝트에서 어떻게 구현했나요?**

```
HIL(Hardware-in-the-Loop) 시뮬레이터:
  실제 비행 컴퓨터 하드웨어를 루프 안에 두고,
  나머지 물리 환경(센서, 액추에이터, 비행역학)은 소프트웨어로 시뮬레이션
  
  루프: [물리 모델] → 센서 데이터 → [실제 HW] → 제어 명령 → [물리 모델]

이 프로젝트 구현:
  6-DOF IMU 시뮬레이터 (imu_sim.c):
    - 3축 가속도: 중력 벡터 회전 투영 (ax = -g·sin(pitch))
    - 3축 각속도: 사인파 모델 (ωroll = A·ω·cos(ωt))
    - 가우시안 노이즈: Box-Muller 변환, ICM-42688-P 스펙 기준
    - 100Hz 샘플링 (실제 비행 컴퓨터 수준)

  비행 컴퓨터 시뮬레이션 (hil_demo.c):
    - I2C로 IMU 레지스터 읽기 (MPU-6050 레지스터 맵 사용)
    - CAN 버스로 IMU 데이터 전송 (ID 0x010, 1Mbps)
    - 실측: 100Hz × 10s = 1000 샘플, CAN TX/I2C 트랜잭션 정합

실제 HIL과의 차이점 (솔직하게 답할 것):
  이 구현은 POSIX 파일 디스크립터로 연결한 순수 소프트웨어 시뮬레이션입니다.
  실제 HIL은 비행 컴퓨터 보드(예: VPX 백플레인)와 D-SUB 커넥터로
  물리적으로 연결하며, 실시간 OS에서 결정론적 타이밍을 보장합니다.
```

**Q. 6자유도의 각 DOF가 무엇인지 설명해주세요.**

```
병진 3-DOF:
  X: 전방-후방 (Surge)
  Y: 좌-우    (Sway)
  Z: 상-하    (Heave)

회전 3-DOF:
  Roll  (Φ): X축 중심 회전 — 발사체에서 롤 안정화 중요
  Pitch (Θ): Y축 중심 회전 — 비행 궤도 각도 변경
  Yaw   (Ψ): Z축 중심 회전 — 방위각 변경

센서와 DOF의 관계:
  가속도계: 병진 3-DOF 측정 (+ 중력 분력)
  자이로:   회전 3-DOF의 각속도 측정
  → 둘을 결합하면 6-DOF 상태 추정 가능 (칼만 필터 등)

코드: imu_sensor.h의 imu_data_t, flight_state_t 구조체
```

---

## 3. 취준생 학습 로드맵

### 3.1 발사체 도메인 지식 (★★★ 최우선)

**기초 개념 (1개월)**
```
[ ] 발사체 시스템 구성:
    - 추진계: 고체/액체 추진제 차이, 추력-시간 선도
    - 구조계: 격벽, 페이로드 페어링, 스테이지 분리
    - 유도제어계: 센서(IMU/GPS) → 항법 컴퓨터 → TVC/핀
    - 텔레메트리: 지상국 ↔ 탑재체 양방향 통신

[ ] 비행역학 기초:
    - 좌표계: 지구 좌표계 vs 기체 좌표계 vs 항법 좌표계
    - 오일러 각도: Roll/Pitch/Yaw와 회전 행렬
    - 쿼터니언: 짐벌 락 문제 해결, 비선형 자세 표현
    - 항법 방정식: INS(관성 항법 시스템) 기초

[ ] 유도·제어:
    - PID 제어기: 비행체 자세 제어 루프
    - TVC(Thrust Vector Control): 추력벡터 제어 원리
    - GNC(Guidance, Navigation, Control): 3가지 서브시스템
```

**추천 학습 자료**
```
📗 "Rocket Propulsion Elements" - Sutton & Biblarz (추진계 바이블)
📗 "Fundamentals of Spacecraft Attitude Determination and Control" - Markley
📗 한국항공우주연구원(KARI) 기술 보고서 (무료 공개)
🎥 MIT OpenCourseWare 16.07 Dynamics (비행역학 강의)
🎥 Scott Manley YouTube - 발사체 공학 쉽게 설명
```

### 3.2 Qt GUI 프레임워크 (★★ 권장)

**지상 관제 소프트웨어(GCS) 개발에 필수**

```
학습 순서:
[ ] Qt 기초: QWidget, Signal/Slot, QThread
[ ] Qt Charts: 실시간 텔레메트리 그래프 (QLineSeries, QChart)
[ ] QSerialPort: UART/RS-422 통신
[ ] QCanBus: CAN 버스 모니터링 (Qt SerialBus 모듈)
[ ] QNetworkAccessManager: HTTP 텔레메트리 업로드

포트폴리오 연계 아이디어:
  sentinel-stack의 HIL 데모를 Qt GCS로 시각화:
  - 롤/피치/요 실시간 그래프 (QChart)
  - CAN 버스 프레임 모니터 (QTableView)
  - 이상 탐지 경보 (QMessageBox/LED 위젯)
  - RS-422 텔레메트리 수신 (QSerialPort)
```

**예시 GCS 아키텍처**
```
┌──────────────────────────────────────┐
│  Qt GCS 애플리케이션                  │
│  ┌─────────┐  ┌─────────┐           │
│  │ IMU Plot │  │ CAN Mon │           │
│  │ (Chart)  │  │ (Table) │           │
│  └─────────┘  └─────────┘           │
│        ↑              ↑             │
│  QSerialPort     QCanBus            │
│  (RS-422 수신)  (CAN 수신)          │
└──────────────────────────────────────┘
         ↕ UART/CAN
┌──────────────────────────────────────┐
│  sentinel-stack HIL 시뮬레이터        │
│  (hil_demo, hal_uart, hal_can)       │
└──────────────────────────────────────┘
```

### 3.3 ROS(Robot Operating System) 기반 개발 (★★ 권장)

**드론, 위성 자세 제어, 무인 이동체에서 활용**

```
학습 순서:
[ ] ROS 2 기초:
    - 노드(Node), 토픽(Topic), 서비스(Service), 액션(Action)
    - rclcpp: C++ 클라이언트 라이브러리
    - colcon 빌드 시스템, package.xml

[ ] 중요 패키지:
    - sensor_msgs: IMU, GPS, Image 표준 메시지 타입
    - geometry_msgs: Pose, Twist, Quaternion
    - nav_msgs: Odometry (위치/속도 통합)
    - tf2: 좌표계 변환 (기체 → 항법 좌표계)

[ ] 항공우주 연계:
    - mavros: MAVLink ↔ ROS 브리지 (드론 FC 연동)
    - px4_msgs: PX4 비행 컨트롤러 메시지
    - robot_localization: EKF 기반 위치 추정

포트폴리오 연계 아이디어:
  imu_sim.c → ROS 2 노드로 포팅
  sensor_msgs/Imu 메시지 발행 → rviz2로 시각화
  → "ROS 2 기반 HIL 시뮬레이터 구현" 이력 생성
```

### 3.4 실제 RTOS 포팅 (★★★ 핵심 보완)

**현재 프로젝트의 가장 큰 약점 보완**

```
현재 상태:
  POSIX nanosleep() 기반 협조적 스케줄링 시뮬레이션
  → 실제 커널 선점, 인터럽트 처리, 하드웨어 타이머 없음

목표:
[ ] FreeRTOS 포팅 (추천):
    - QEMU vexpress-a9 보드에서 FreeRTOS 실행
    - rms.c, edf.c 알고리즘을 FreeRTOS 태스크로 이식
    - vTaskDelay() + xTaskGetTickCount() 지터 측정
    - 기존 POSIX 코드와 동일한 결과 비교

[ ] Zephyr RTOS (대안):
    - STM32 개발 보드 또는 QEMU 타겟
    - K_THREAD_DEFINE(), k_msleep() 사용
    - 실제 하드웨어에서 UART/I2C/SPI 드라이버 검증

면접 어필 포인트:
  "POSIX 시뮬레이션으로 알고리즘을 검증한 후
  FreeRTOS로 동일한 스케줄러를 포팅하여
  실제 선점형 커널에서의 동작을 확인했습니다."
```

### 3.5 6-DOF 비행 시뮬레이터 심화 (★ 선택)

**현재 구현(사인파 모델)에서 실제 물리 모델로 업그레이드**

```
현재 모델:
  roll(t) = A·sin(ωt)  ← 사인파 근사

실제 6-DOF 모델:
  [ ] 뉴턴-오일러 운동 방정식:
      dv/dt = (1/m) × (F_thrust + F_drag + F_gravity)
      dω/dt = I⁻¹ × (τ - ω × (I·ω))
      여기서: m=질량, I=관성 텐서, τ=토크

  [ ] 수치 적분:
      4차 룽게-쿠타(RK4) 방법으로 상태 방정식 적분
      Δt = 1ms (항법 루프 주파수 1kHz)

  [ ] 대기 모델:
      ICAO 표준 대기 (고도별 밀도, 온도, 압력)
      항력 계수 Cd (마하 수에 따른 가변)

학습 자료:
  "Modeling and Simulation of Aerospace Vehicle Dynamics" - Zipfel
  JSBSim (오픈소스 비행 역학 모델) 코드 분석
```

### 3.6 텔레메트리 DB 설계 (★ 선택)

```
현재: CSV 파일 출력
목표: 임베디드 DB로 텔레메트리 저장

[ ] SQLite (추천):
    - 임베디드 환경 표준
    - 발사체 파라미터, 텔레메트리 히스토리 저장
    - sentinel-stack 연동: reporter.c → SQLite

[ ] InfluxDB / TimescaleDB (선택):
    - 시계열 DB: 센서 데이터에 최적
    - 지상국 모니터링에 활용
```

---

## 4. 학습 우선순위 및 타임라인

### 취업 목표별 우선순위

**목표 A: 발사체 탑재 SW 개발자 (이 JD)**
```
Month 1-2: 발사체 도메인 지식 (섹션 3.1) ← 지식 차별화
Month 2-3: FreeRTOS 실제 포팅 (섹션 3.4) ← 기술 차별화
Month 3-4: Qt GCS 개발 (섹션 3.2)        ← 우대사항 달성
Month 4-6: 6-DOF 물리 모델 (섹션 3.5)    ← 상급 차별화
```

**목표 B: 항공 임베디드 SW (항공기 FCS/FMS)**
```
Month 1-2: DO-178C 심화 + MC/DC 커버리지
Month 2-3: ARINC 664(AFDX) 프로토콜 학습
Month 3-4: VxWorks / Integrity RTOS 학습
Month 4-6: Qt + GCS 개발
```

**목표 C: 드론/무인 이동체**
```
Month 1-2: ROS 2 기초 (섹션 3.3)
Month 2-3: PX4/ArduPilot 코드 분석
Month 3-4: MAVLink 프로토콜 구현
Month 4-6: 자율 항법 알고리즘 (EKF, A*)
```

---

## 5. sentinel-stack이 이미 보여주는 것 (자기소개서 표현)

```
[사용 가능한 표현 — 코드로 뒷받침됨]

"UART/RS-422/I2C/SPI/CAN 인터페이스에 대한 HAL 추상화 레이어를 설계하여
  실제 MCU 드라이버로 교체 가능한 이식성 높은 구조를 구현했습니다."
  → 근거: hal/include/*.h, hal/src/*_posix.c

"6-DOF IMU 시뮬레이터를 구현하여 비행 컴퓨터의 I2C 읽기부터
  CAN 버스 텔레메트리 전송까지의 데이터 파이프라인을 검증했습니다."
  → 근거: hil-simulator/src/imu_sim.c, hil_demo.c

"ICM-42688-P 급 IMU의 가속도계·자이로 노이즈 특성을 Box-Muller 변환으로
  모델링하여 100Hz 실시간 센서 데이터를 생성했습니다."
  → 근거: imu_sim.c — gaussian_noise()

"CAN ID 우선순위 체계를 발사체 시스템 요구사항에 맞게 설계했습니다.
  비행제어 명령(0x001) > IMU(0x010) > GPS(0x020) > 텔레메트리(0x100)"
  → 근거: hal_can.h — CAN_ID_* 상수 정의
```

---

## 6. 면접 예상 추가 질문 (신규)

**Q. CAN 버스 중재(Arbitration) 원리를 설명하세요.**

```
비파괴 중재 과정:
1. 모든 노드가 동시에 전송 시작
2. 각 비트를 전송하며 버스 전압 모니터링
3. Dominant(0)이 Recessive(1)를 이김
   → ID 비트 중 첫 번째 0이 나오는 노드가 버스 획득
4. 중재에서 진 노드: 즉시 전송 중단, 버스 유휴 후 재시도
5. 충돌 없음: 이긴 노드는 전송 완료 후 버스 해제

따라서: 낮은 ID = 더 많은 0비트 = 버스 우선권 높음
```

**Q. IMU 데이터에서 Roll/Pitch는 어떻게 계산하나요?**

```
가속도계만으로 계산 (정적):
  pitch = arctan(ax / sqrt(ay² + az²))
  roll  = arctan(ay / az)

한계:
  - 동적 가속도(선형 가속) 있으면 오차 발생
  - 자이로 드리프트(장시간 누적 오차)

실제 솔루션: 센서 퓨전
  - 상보 필터: angle = α × (angle + gyro_rate × dt) + (1-α) × accel_angle
  - 칼만 필터: 최적 추정기, 가속도계 + 자이로 + 모델 통합

이 프로젝트: imu_sim.c의 flight_state_t에서
  sim.state.roll, .pitch, .yaw — 진값 (ground truth)
  out.accel_x, .gyro_x — 노이즈 포함 측정값
  → 두 값의 차이가 센서 퓨전 필요성을 보여줌
```

**Q. 발사체 탑재 SW에서 malloc을 절대 사용하면 안 되는 이유는?**

```
3가지 이유:

1. 결정론성(Determinism) 파괴
   malloc 실행 시간은 힙 상태에 따라 가변
   → WCET 분석 불가능 → 스케줄 가능성 보장 불가

2. 힙 단편화(Fragmentation)
   장시간 비행 중 힙 단편화 누적
   → 특정 시점에 할당 실패 → 시스템 장애
   → 발사 중 메모리 부족 = 임무 실패

3. 인증 불가
   DO-178C Level A: 완전한 코드 커버리지 필요
   malloc/free 실패 경로를 완전 커버 불가능

해결책: 정적 메모리 풀
  rtos_task_t tasks[MAX_TASKS] = {0};   // 컴파일 타임에 크기 결정
  sentinel_packet_t pkt_pool[16] = {0}; // 스택 또는 정적 영역

이 프로젝트: task.h의 모든 구조체 — 정적 배열 기반
```
