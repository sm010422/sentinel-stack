/**
 * @file hil_demo.c
 * @brief HIL 시뮬레이터 통합 데모
 *
 * RTOS 스케줄러 + HAL I2C/SPI + IMU 시뮬레이터를 통합하는 데모입니다.
 *
 * 시뮬레이션 아키텍처:
 *
 *   ┌─────────────────────────────────────────────────────┐
 *   │               HIL 시뮬레이터 (이 파일)               │
 *   │                                                     │
 *   │  ┌──────────────────┐    I2C 버스    ┌───────────┐  │
 *   │  │   IMU 시뮬레이터  │ ←─────────── │ 비행 컴퓨터 │  │
 *   │  │  (imu_sim.c)    │   레지스터 읽기 │ (RTOS 태스크)│ │
 *   │  └──────────────────┘               └───────────┘  │
 *   │                                           ↓         │
 *   │                                    CAN 버스 전송     │
 *   │                                    (CAN ID: 0x010)  │
 *   └─────────────────────────────────────────────────────┘
 *
 * 실제 HIL 환경과의 차이:
 *  실제: 비행 컴퓨터(HW) ↔ UART/SPI ↔ 센서 시뮬레이터(SW)
 *  이 데모: 모두 소프트웨어, POSIX 파일 디스크립터로 연결
 *
 * RTOS 태스크 구성:
 *  - IMU_READ_TASK: 10ms 주기, 우선순위 0 (최고) — ICM-42688 레지스터 읽기
 *  - CAN_TX_TASK:   10ms 주기, 우선순위 1 — IMU 데이터 CAN 버스 전송
 *  - STATUS_TASK:   100ms 주기, 우선순위 2 — 비행 상태 출력
 *
 * @author  Park Sang Min
 * @date    2025
 */

#define _POSIX_C_SOURCE 200809L

#include "imu_sensor.h"
#include "hal_i2c.h"
#include "hal_can.h"
#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ── 시뮬레이션 설정 ───────────────────────────────────────────────────── */
#define SIM_DURATION_S      10      /**< 시뮬레이션 실행 시간 (초) */
#define PRINT_INTERVAL_MS   500     /**< 출력 주기 (ms) */
#define MPU6050_ADDR        0x68U   /**< I2C 장치 주소 */

/* MPU-6050 레지스터 주소 (ICM-42688과 호환 서브셋) */
#define REG_ACCEL_XOUT_H  0x3BU
#define REG_ACCEL_XOUT_L  0x3CU
#define REG_ACCEL_YOUT_H  0x3DU
#define REG_ACCEL_YOUT_L  0x3EU
#define REG_ACCEL_ZOUT_H  0x3FU
#define REG_ACCEL_ZOUT_L  0x40U
#define REG_GYRO_XOUT_H   0x43U
#define REG_GYRO_XOUT_L   0x44U
#define REG_GYRO_YOUT_H   0x45U
#define REG_GYRO_YOUT_L   0x46U
#define REG_GYRO_ZOUT_H   0x47U
#define REG_GYRO_ZOUT_L   0x48U

/* ── 전역 상태 ─────────────────────────────────────────────────────────── */
static volatile int g_running = 1;

static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ── 유틸리티: float → 16비트 raw 값 변환 ─────────────────────────────── */
static int16_t accel_to_raw(float ms2)
{
    /* ±16g 범위, 16비트 → LSB = 156.9/32768 m/s² ≈ 0.00479 m/s² */
    float lsb = ms2 / (IMU_ACCEL_RANGE_MS2 / 32768.0f);
    if (lsb > 32767.0f)  { lsb = 32767.0f; }
    if (lsb < -32768.0f) { lsb = -32768.0f; }
    return (int16_t)lsb;
}

static int16_t gyro_to_raw(float rps)
{
    /* ±2000dps = ±34.9 rad/s 범위 */
    float lsb = rps / (IMU_GYRO_RANGE_RPS / 32768.0f);
    if (lsb > 32767.0f)  { lsb = 32767.0f; }
    if (lsb < -32768.0f) { lsb = -32768.0f; }
    return (int16_t)lsb;
}

/* ── IMU 레지스터 갱신 (시뮬레이터 → I2C 레지스터 파일 쓰기) ────────────── */
static void update_imu_registers(hal_i2c_t *i2c, const imu_data_t *data)
{
    int16_t ax = accel_to_raw(data->accel_x);
    int16_t ay = accel_to_raw(data->accel_y);
    int16_t az = accel_to_raw(data->accel_z);
    int16_t gx = gyro_to_raw(data->gyro_x);
    int16_t gy = gyro_to_raw(data->gyro_y);
    int16_t gz = gyro_to_raw(data->gyro_z);

    /* 빅엔디안 레지스터 쓰기 (MPU-6050 사양) */
    uint8_t buf[2];

    buf[0] = (uint8_t)((ax >> 8) & 0xFF); buf[1] = (uint8_t)(ax & 0xFF);
    hal_i2c_write_reg(i2c, MPU6050_ADDR, REG_ACCEL_XOUT_H, buf, 2);

    buf[0] = (uint8_t)((ay >> 8) & 0xFF); buf[1] = (uint8_t)(ay & 0xFF);
    hal_i2c_write_reg(i2c, MPU6050_ADDR, REG_ACCEL_YOUT_H, buf, 2);

    buf[0] = (uint8_t)((az >> 8) & 0xFF); buf[1] = (uint8_t)(az & 0xFF);
    hal_i2c_write_reg(i2c, MPU6050_ADDR, REG_ACCEL_ZOUT_H, buf, 2);

    buf[0] = (uint8_t)((gx >> 8) & 0xFF); buf[1] = (uint8_t)(gx & 0xFF);
    hal_i2c_write_reg(i2c, MPU6050_ADDR, REG_GYRO_XOUT_H, buf, 2);

    buf[0] = (uint8_t)((gy >> 8) & 0xFF); buf[1] = (uint8_t)(gy & 0xFF);
    hal_i2c_write_reg(i2c, MPU6050_ADDR, REG_GYRO_YOUT_H, buf, 2);

    buf[0] = (uint8_t)((gz >> 8) & 0xFF); buf[1] = (uint8_t)(gz & 0xFF);
    hal_i2c_write_reg(i2c, MPU6050_ADDR, REG_GYRO_ZOUT_H, buf, 2);
}

/* ── 비행 컴퓨터 시뮬레이션: I2C 레지스터 읽기 → CAN 전송 ───────────────── */
static void flight_computer_tick(hal_i2c_t *i2c, hal_can_t *can,
                                  uint32_t tick_count)
{
    /* I2C에서 IMU 데이터 읽기 (비행 컴퓨터가 하는 작업) */
    uint8_t raw[12];
    if (hal_i2c_read_reg(i2c, MPU6050_ADDR, REG_ACCEL_XOUT_H,
                          raw, sizeof(raw)) != HAL_OK) {
        return;
    }

    /* raw → 공학 단위 변환 */
    int16_t ax = (int16_t)(((uint16_t)raw[0]  << 8) | raw[1]);
    int16_t ay = (int16_t)(((uint16_t)raw[2]  << 8) | raw[3]);
    int16_t az = (int16_t)(((uint16_t)raw[4]  << 8) | raw[5]);
    int16_t gx = (int16_t)(((uint16_t)raw[6]  << 8) | raw[7]);
    int16_t gy = (int16_t)(((uint16_t)raw[8]  << 8) | raw[9]);
    int16_t gz = (int16_t)(((uint16_t)raw[10] << 8) | raw[11]);

    /* CAN 전송: IMU_DATA 프레임 (8바이트: ax,ay,az,gx — int8_t 정규화) */
    hal_can_frame_t frame = {
        .id             = CAN_ID_IMU_DATA,
        .is_extended_id = false,
        .dlc            = 8U,
    };

    /* 8비트 압축 (CAN DLC 제약: 8바이트 최대) */
    frame.data[0] = (uint8_t)((ax / 128) + 128);  /* -128~127 → 0~255 */
    frame.data[1] = (uint8_t)((ay / 128) + 128);
    frame.data[2] = (uint8_t)((az / 128) + 128);
    frame.data[3] = (uint8_t)((gx / 256) + 128);
    frame.data[4] = (uint8_t)((gy / 256) + 128);
    frame.data[5] = (uint8_t)((gz / 256) + 128);
    frame.data[6] = (uint8_t)((tick_count >> 8) & 0xFFU);  /* 틱 카운터 MSB */
    frame.data[7] = (uint8_t)( tick_count       & 0xFFU);  /* 틱 카운터 LSB */

    hal_can_write(can, &frame, 10);
}

/* ── 메인 ──────────────────────────────────────────────────────────────── */
int main(void)
{
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    printf("════════════════════════════════════════════════════\n");
    printf("  sentinel-stack HIL 시뮬레이터\n");
    printf("  6-DOF IMU (ICM-42688-P) + I2C + CAN 버스\n");
    printf("  Ctrl+C로 종료\n");
    printf("════════════════════════════════════════════════════\n\n");

    /* ── 초기화 ──────────────────────────────────────────────────────── */
    imu_sim_t sim;
    imu_sim_init(&sim);

    /* 발사체 초기 흔들림 모사: 롤 0.8Hz, 피치 0.5Hz, 요 0.3Hz */
    imu_sim_set_trajectory(&sim, 0.8f, 0.5f, 0.3f);

    /* assert() 안에 초기화 함수를 넣으면 Release(NDEBUG)에서 호출 안 됨 */
    hal_i2c_t i2c;
    if (hal_i2c_init(&i2c, HAL_I2C_CLOCK_FAST) != HAL_OK) {
        fprintf(stderr, "I2C 초기화 실패\n"); return 1;
    }
    if (hal_i2c_register_device(&i2c, MPU6050_ADDR) != HAL_OK) {
        fprintf(stderr, "I2C 장치 등록 실패\n"); return 1;
    }

    hal_can_t can;
    if (hal_can_init(&can, HAL_CAN_BAUD_1M, true) != HAL_OK) {
        fprintf(stderr, "CAN 초기화 실패\n"); return 1;
    }

    printf("%-8s %-10s %-10s %-10s %-10s %-10s %-10s  %s\n",
           "Time(s)", "Ax(m/s²)", "Ay(m/s²)", "Az(m/s²)",
           "Gx(°/s)", "Gy(°/s)", "Gz(°/s)", "Seq");
    printf("────────────────────────────────────────────────────────────────\n");

    /* ── 메인 루프: 100Hz 시뮬레이션 ─────────────────────────────────── */
    const float   DT_S = 1.0f / (float)IMU_SAMPLE_RATE_HZ;
    const struct timespec TICK = { .tv_sec = 0, .tv_nsec = IMU_PERIOD_US * 1000L };

    uint32_t tick       = 0U;
    uint32_t print_tick = (uint32_t)(PRINT_INTERVAL_MS / IMU_PERIOD_MS);

    while (g_running &&
           (float)tick * DT_S < (float)SIM_DURATION_S) {

        imu_data_t data;
        imu_sim_step(&sim, DT_S, &data);

        /* IMU 시뮬레이터 → I2C 레지스터 갱신 */
        update_imu_registers(&i2c, &data);

        /* 비행 컴퓨터 로직: I2C 읽기 → CAN 전송 */
        flight_computer_tick(&i2c, &can, tick);

        /* 상태 출력 (500ms마다) */
        if (tick % print_tick == 0U) {
            if (!imu_data_validate(&data)) {
                fprintf(stderr, "[ERROR] IMU 데이터 범위 초과 (seq=%u)\n",
                        data.sequence);
            }
            printf("%-8.2f %-10.4f %-10.4f %-10.4f %-10.4f %-10.4f %-10.4f  %u\n",
                   (float)tick * DT_S,
                   (double)data.accel_x, (double)data.accel_y, (double)data.accel_z,
                   (double)(data.gyro_x * IMU_RAD_TO_DEG),
                   (double)(data.gyro_y * IMU_RAD_TO_DEG),
                   (double)(data.gyro_z * IMU_RAD_TO_DEG),
                   data.sequence);
        }

        tick++;
        nanosleep(&TICK, NULL);
    }

    /* ── 통계 출력 ───────────────────────────────────────────────────── */
    printf("\n════════════════════════════════════════════════════\n");
    printf("  시뮬레이션 완료\n");
    printf("  총 샘플 수:     %u\n",   tick);
    printf("  경과 시간:      %.2fs\n", (float)tick * DT_S);
    printf("  CAN TX 프레임:  %u\n",   can.tx_count);
    printf("  I2C 트랜잭션:   %u\n",   i2c.transaction_count);
    printf("════════════════════════════════════════════════════\n");

    hal_can_deinit(&can);
    hal_i2c_deinit(&i2c);
    return 0;
}
