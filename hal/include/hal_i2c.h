/**
 * @file hal_i2c.h
 * @brief I2C 하드웨어 추상화 인터페이스
 *
 * I2C(Inter-Integrated Circuit): Phillips가 개발한 2선 직렬 버스
 *  - SCL(클럭)  + SDA(데이터) 2개 신호선
 *  - 표준 모드: 100kHz / 빠른 모드: 400kHz / 고속 모드: 3.4MHz
 *  - 주소 공간: 7비트(128개) 또는 10비트 장치 주소
 *  - 마스터-슬레이브 구조, 클럭 스트레칭 지원
 *
 * 항공우주 활용 예:
 *  - MPU-6050 (IMU 센서) → I2C 주소 0x68/0x69
 *  - BMP280 (기압계)     → I2C 주소 0x76/0x77
 *  - EEPROM (파라미터 저장) → I2C 주소 0x50-0x57
 *
 * POSIX 시뮬레이션: 공유 메모리 레지스터 파일 방식
 *  실제 구현:
 *    STM32 → HAL_I2C_Mem_Write/Read
 *    Linux → /dev/i2c-*, ioctl(I2C_RDWR)
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_HAL_I2C_H
#define SENTINEL_HAL_I2C_H

#include "hal_common.h"

/* ── I2C 파라미터 ──────────────────────────────────────────────────────── */
#define HAL_I2C_CLOCK_STANDARD  100000U   /**< 100kHz 표준 모드 */
#define HAL_I2C_CLOCK_FAST      400000U   /**< 400kHz 고속 모드 */
#define HAL_I2C_CLOCK_FAST_PLUS 1000000U  /**< 1MHz Fast-mode Plus */

#define HAL_I2C_MAX_DEVICES     128U  /**< 7비트 주소 공간 */
#define HAL_I2C_REG_SIZE        256U  /**< 장치당 레지스터 공간 (bytes) */
#define HAL_I2C_MAX_TRANSFER    32U   /**< 단일 전송 최대 크기 */

/* ── I2C 버스 제어 블록 ────────────────────────────────────────────────── */
typedef struct {
    uint32_t clock_hz;    /**< 버스 클럭 주파수 */
    bool     initialized;

    /* POSIX 시뮬레이션: 레지스터 파일
     * 인덱스: [장치주소 0~127][레지스터주소 0~255]
     * 실제 하드웨어에서는 물리 I2C 버스로 교체
     */
    uint8_t  regs[HAL_I2C_MAX_DEVICES][HAL_I2C_REG_SIZE];
    bool     device_present[HAL_I2C_MAX_DEVICES];

    /* 통계 */
    uint32_t transaction_count;
    uint32_t nack_count;
    uint32_t timeout_count;
} hal_i2c_t;

/* ── API ───────────────────────────────────────────────────────────────── */

/** @brief I2C 버스 초기화 */
hal_status_t hal_i2c_init(hal_i2c_t *i2c, uint32_t clock_hz);

/**
 * @brief 시뮬레이션용 슬레이브 장치 등록
 * @note 실제 하드웨어에서는 불필요 (물리 장치가 버스에 존재)
 */
hal_status_t hal_i2c_register_device(hal_i2c_t *i2c, uint8_t dev_addr);

/**
 * @brief 슬레이브 레지스터 쓰기
 * @param dev_addr  7비트 장치 주소 (예: 0x68 for MPU-6050)
 * @param reg_addr  레지스터 주소
 * @param data      쓸 데이터
 * @param len       데이터 길이
 */
hal_status_t hal_i2c_write_reg(hal_i2c_t *i2c, uint8_t dev_addr,
                                uint8_t reg_addr,
                                const uint8_t *data, size_t len);

/**
 * @brief 슬레이브 레지스터 읽기
 * @param dev_addr  7비트 장치 주소
 * @param reg_addr  시작 레지스터 주소
 * @param data      읽은 데이터 저장 버퍼
 * @param len       읽을 바이트 수
 */
hal_status_t hal_i2c_read_reg(hal_i2c_t *i2c, uint8_t dev_addr,
                               uint8_t reg_addr,
                               uint8_t *data, size_t len);

/**
 * @brief 슬레이브 존재 확인 (I2C probe)
 * @return HAL_OK: 응답 있음, HAL_ERR_NACK: 응답 없음
 */
hal_status_t hal_i2c_probe(const hal_i2c_t *i2c, uint8_t dev_addr);

/** @brief I2C 버스 해제 */
void hal_i2c_deinit(hal_i2c_t *i2c);

#endif /* SENTINEL_HAL_I2C_H */
