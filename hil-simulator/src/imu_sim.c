/**
 * @file imu_sim.c
 * @brief 6-DOF IMU 시뮬레이터 구현
 *
 * 발사체/항공기의 6자유도 운동을 시뮬레이션하고
 * ICM-42688-P 급의 가속도계 + 자이로스코프 데이터를 생성합니다.
 *
 * 운동 모델:
 *  - 자세 변화: 사인파 모델 (각 축별 독립 주파수/진폭)
 *  - 가속도 계산: 회전 좌표계에서 중력 투영 + 추력 가속도
 *  - 각속도: 자세 변화율 (sin → 미분 → cos × 각주파수)
 *  - 노이즈: Box-Muller 변환을 이용한 가우시안 노이즈
 *
 * 이 모델이 방산 면접에서 설명하기 좋은 이유:
 *  실제 HIL 시뮬레이터는 6-DOF 비선형 방정식을 수치 적분하지만,
 *  이 프로젝트는 RTOS 태스크 스케줄링과 HAL 인터페이스 검증이 목적이므로
 *  운동 모델을 단순화하고 인터페이스 정확도에 집중합니다.
 */

#define _POSIX_C_SOURCE 200809L

#include "imu_sensor.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── 내부 유틸리티 ─────────────────────────────────────────────────────── */

static float clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

/* Box-Muller 변환: 균등분포 → 표준 정규분포 (σ=1) */
static float gaussian_noise(float std)
{
    if (std <= 0.0f) { return 0.0f; }
    float u1 = ((float)rand() / (float)RAND_MAX) + 1e-7f;
    float u2 =  (float)rand() / (float)RAND_MAX;
    float z  = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    return z * std;
}

static uint64_t monotonic_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000U + (uint64_t)ts.tv_nsec / 1000U;
}

/* ── API 구현 ──────────────────────────────────────────────────────────── */

void imu_sim_init(imu_sim_t *sim)
{
    assert(sim != NULL);

    memset(sim, 0, sizeof(*sim));
    srand((unsigned int)monotonic_us());

    /* 기본 궤도: 완만한 롤/피치 진동 (발사 후 자세 안정화 구간) */
    sim->roll_freq_hz  = 0.5f;
    sim->pitch_freq_hz = 0.3f;
    sim->yaw_freq_hz   = 0.2f;

    sim->roll_amp_rad  = 0.1745f;  /* 10° */
    sim->pitch_amp_rad = 0.0873f;  /* 5° */
    sim->yaw_amp_rad   = 0.0524f;  /* 3° */

    /* ICM-42688-P 저잡음 모드 기준 노이즈 스펙 */
    sim->accel_noise_std = 0.0012f;  /* 1.2 mg RMS = 약 0.012 m/s² */
    sim->gyro_noise_std  = 0.00087f; /* 0.05 dps RMS = 약 0.00087 rad/s */

    sim->initialized = true;
}

void imu_sim_set_trajectory(imu_sim_t *sim,
                             float roll_freq_hz,
                             float pitch_freq_hz,
                             float yaw_freq_hz)
{
    if (sim == NULL) { return; }
    sim->roll_freq_hz  = roll_freq_hz;
    sim->pitch_freq_hz = pitch_freq_hz;
    sim->yaw_freq_hz   = yaw_freq_hz;
}

void imu_sim_step(imu_sim_t *sim, float dt_s, imu_data_t *out)
{
    assert(sim != NULL);
    assert(out  != NULL);
    assert(sim->initialized);

    sim->sim_time_s += dt_s;
    float t = sim->sim_time_s;

    /* ── 자세 계산 (사인파 모델) ─────────────────────────────────────── */
    float wr = 2.0f * (float)M_PI * sim->roll_freq_hz;
    float wp = 2.0f * (float)M_PI * sim->pitch_freq_hz;
    float wy = 2.0f * (float)M_PI * sim->yaw_freq_hz;

    sim->state.roll  = sim->roll_amp_rad  * sinf(wr * t);
    sim->state.pitch = sim->pitch_amp_rad * sinf(wp * t);
    sim->state.yaw   = sim->yaw_amp_rad   * sinf(wy * t);

    /* ── 각속도 계산 (자세 미분) ─────────────────────────────────────── */
    sim->state.roll_rate  = sim->roll_amp_rad  * wr * cosf(wr * t);
    sim->state.pitch_rate = sim->pitch_amp_rad * wp * cosf(wp * t);
    sim->state.yaw_rate   = sim->yaw_amp_rad   * wy * cosf(wy * t);

    /* ── 가속도 계산 (기체 좌표계에서의 중력 투영) ───────────────────── */
    float roll  = sim->state.roll;
    float pitch = sim->state.pitch;

    /* 회전 좌표계에서 중력 g의 분력:
     * - 수평 가속도 (ax, ay): 기울기에 의한 중력 성분
     * - 수직 가속도 (az):     중력 주성분 (양수 = 하방 = 센서 측정값 +g)
     */
    float ax = -IMU_GRAVITY_MS2 * sinf(pitch);
    float ay =  IMU_GRAVITY_MS2 * cosf(pitch) * sinf(roll);
    float az =  IMU_GRAVITY_MS2 * cosf(pitch) * cosf(roll);

    /* ── 센서 노이즈 추가 ────────────────────────────────────────────── */
    ax += gaussian_noise(sim->accel_noise_std);
    ay += gaussian_noise(sim->accel_noise_std);
    az += gaussian_noise(sim->accel_noise_std);

    float gx = sim->state.roll_rate  + gaussian_noise(sim->gyro_noise_std);
    float gy = sim->state.pitch_rate + gaussian_noise(sim->gyro_noise_std);
    float gz = sim->state.yaw_rate   + gaussian_noise(sim->gyro_noise_std);

    /* ── 센서 포화 클램핑 ────────────────────────────────────────────── */
    out->accel_x = clampf(ax, -IMU_ACCEL_RANGE_MS2, IMU_ACCEL_RANGE_MS2);
    out->accel_y = clampf(ay, -IMU_ACCEL_RANGE_MS2, IMU_ACCEL_RANGE_MS2);
    out->accel_z = clampf(az, -IMU_ACCEL_RANGE_MS2, IMU_ACCEL_RANGE_MS2);
    out->gyro_x  = clampf(gx, -IMU_GYRO_RANGE_RPS,  IMU_GYRO_RANGE_RPS);
    out->gyro_y  = clampf(gy, -IMU_GYRO_RANGE_RPS,  IMU_GYRO_RANGE_RPS);
    out->gyro_z  = clampf(gz, -IMU_GYRO_RANGE_RPS,  IMU_GYRO_RANGE_RPS);

    out->timestamp_us = monotonic_us();
    out->sequence     = sim->sequence++;
    out->valid        = true;
}

const flight_state_t *imu_sim_get_state(const imu_sim_t *sim)
{
    if (sim == NULL || !sim->initialized) { return NULL; }
    return &sim->state;
}

bool imu_data_validate(const imu_data_t *data)
{
    if (data == NULL || !data->valid) { return false; }

    /* 가속도계 범위 검사 */
    if (fabsf(data->accel_x) > IMU_ACCEL_RANGE_MS2) { return false; }
    if (fabsf(data->accel_y) > IMU_ACCEL_RANGE_MS2) { return false; }
    if (fabsf(data->accel_z) > IMU_ACCEL_RANGE_MS2) { return false; }

    /* 자이로 범위 검사 */
    if (fabsf(data->gyro_x) > IMU_GYRO_RANGE_RPS) { return false; }
    if (fabsf(data->gyro_y) > IMU_GYRO_RANGE_RPS) { return false; }
    if (fabsf(data->gyro_z) > IMU_GYRO_RANGE_RPS) { return false; }

    return true;
}
