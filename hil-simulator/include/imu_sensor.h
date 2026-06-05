/**
 * @file imu_sensor.h
 * @brief 6자유도(6-DOF) IMU 센서 시뮬레이터 인터페이스
 *
 * HIL(Hardware-in-the-Loop) 시뮬레이터의 핵심 센서 모듈입니다.
 *
 * 6-DOF(6 Degrees of Freedom):
 *  - 3 병진 자유도: X(전후), Y(좌우), Z(상하) 가속도
 *  - 3 회전 자유도: Roll(옆 회전), Pitch(앞뒤 기울기), Yaw(수평 회전)
 *
 * 실제 발사체/항공기에서의 IMU:
 *  - 발사체 RCS 제어: IMU → 항법 컴퓨터 → TVC(추력벡터제어) 명령
 *  - 항공기 FCS: IMU → FBW(Fly-by-Wire) 컴퓨터 → 제어면 구동
 *
 * 시뮬레이션 모델:
 *  - 6-DOF 강체 운동 방정식 (뉴턴-오일러 공식화)
 *  - 가우시안 노이즈 (실제 센서 노이즈 모델링)
 *  - 중력 벡터 보정
 *
 * 참고 센서:
 *  - ICM-42688-P: SpaceX Starlink 위성 자세 제어용 고정밀 IMU
 *  - ADIS16448: 방산 등급 전술 IMU
 *  - MPU-6050: 소형 위성/드론 범용 IMU
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_IMU_SENSOR_H
#define SENTINEL_IMU_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/* ── IMU 샘플링 설정 ───────────────────────────────────────────────────── */
#define IMU_SAMPLE_RATE_HZ    100U   /**< 100Hz 샘플링 (10ms 주기) */
#define IMU_PERIOD_MS         10U    /**< RTOS 태스크 주기 */
#define IMU_PERIOD_US         10000U /**< RTOS 태스크 주기 (µs) */

/* ── 물리 상수 ─────────────────────────────────────────────────────────── */
#define IMU_GRAVITY_MS2       9.80665f  /**< 표준 중력 가속도 (m/s²) */
#define IMU_DEG_TO_RAD        0.017453f /**< 도 → 라디안 변환 계수 */
#define IMU_RAD_TO_DEG        57.2958f  /**< 라디안 → 도 변환 계수 */

/* ── 센서 범위 (ICM-42688-P 기준) ─────────────────────────────────────── */
#define IMU_ACCEL_RANGE_MS2   156.9f   /**< ±16g = 156.9 m/s² */
#define IMU_GYRO_RANGE_RPS    34.9f    /**< ±2000 dps = 34.9 rad/s */

/* ── 6-DOF 센서 데이터 ─────────────────────────────────────────────────── */
typedef struct {
    /* 가속도계 (m/s²) — 비행체 기체 좌표계 */
    float accel_x;   /**< X축 가속도 (전방) */
    float accel_y;   /**< Y축 가속도 (우측) */
    float accel_z;   /**< Z축 가속도 (하방, 정지 시 ≈ +9.81 m/s²) */

    /* 자이로스코프 (rad/s) — 기체 좌표계 */
    float gyro_x;    /**< X축 각속도 (롤 레이트) */
    float gyro_y;    /**< Y축 각속도 (피치 레이트) */
    float gyro_z;    /**< Z축 각속도 (요 레이트) */

    /* 메타데이터 */
    uint64_t timestamp_us;  /**< 측정 시각 (CLOCK_MONOTONIC 기준, µs) */
    uint32_t sequence;      /**< 시퀀스 번호 (누락 검출용) */
    bool     valid;         /**< 데이터 유효성 플래그 */
} imu_data_t;

/* ── 6-DOF 비행체 상태 ─────────────────────────────────────────────────── */
typedef struct {
    /* 위치 (m) */
    float pos_x, pos_y, pos_z;

    /* 속도 (m/s) */
    float vel_x, vel_y, vel_z;

    /* 자세 (rad) — 오일러 각도 */
    float roll;   /**< 롤: X축 중심 회전 */
    float pitch;  /**< 피치: Y축 중심 회전 */
    float yaw;    /**< 요: Z축 중심 회전 */

    /* 각속도 (rad/s) */
    float roll_rate;
    float pitch_rate;
    float yaw_rate;
} flight_state_t;

/* ── IMU 시뮬레이터 제어 블록 ──────────────────────────────────────────── */
typedef struct {
    flight_state_t state;         /**< 현재 비행 상태 */

    /* 궤도 파라미터 (사인파 모델) */
    float roll_freq_hz;   /**< 롤 진동 주파수 */
    float pitch_freq_hz;  /**< 피치 진동 주파수 */
    float yaw_freq_hz;    /**< 요 진동 주파수 */

    float roll_amp_rad;   /**< 롤 진폭 (rad) */
    float pitch_amp_rad;  /**< 피치 진폭 (rad) */
    float yaw_amp_rad;    /**< 요 진폭 (rad) */

    /* 노이즈 모델 */
    float accel_noise_std;  /**< 가속도계 노이즈 표준편차 (m/s²) */
    float gyro_noise_std;   /**< 자이로 노이즈 표준편차 (rad/s) */

    /* 내부 상태 */
    float    sim_time_s;    /**< 시뮬레이션 경과 시간 (초) */
    uint32_t sequence;      /**< 출력 시퀀스 번호 */
    bool     initialized;
} imu_sim_t;

/* ── API ───────────────────────────────────────────────────────────────── */

/**
 * @brief IMU 시뮬레이터 초기화 (기본 파라미터)
 * @param sim 시뮬레이터 제어 블록
 */
void imu_sim_init(imu_sim_t *sim);

/**
 * @brief 궤도 파라미터 설정
 * @param roll_freq_hz   롤 진동 주파수 (예: 0.5 Hz)
 * @param pitch_freq_hz  피치 진동 주파수 (예: 0.3 Hz)
 * @param yaw_freq_hz    요 진동 주파수 (예: 0.2 Hz)
 */
void imu_sim_set_trajectory(imu_sim_t *sim,
                             float roll_freq_hz,
                             float pitch_freq_hz,
                             float yaw_freq_hz);

/**
 * @brief 시뮬레이션 1스텝 진행 — IMU 데이터 출력
 * @param sim   시뮬레이터
 * @param dt_s  타임스텝 (초, 일반적으로 0.01 = 100Hz)
 * @param out   출력 IMU 데이터
 */
void imu_sim_step(imu_sim_t *sim, float dt_s, imu_data_t *out);

/**
 * @brief 현재 비행체 상태 조회
 */
const flight_state_t *imu_sim_get_state(const imu_sim_t *sim);

/**
 * @brief IMU 데이터 유효성 검사
 * @return true: 정상 범위, false: 센서 포화 또는 이상값
 */
bool imu_data_validate(const imu_data_t *data);

#endif /* SENTINEL_IMU_SENSOR_H */
