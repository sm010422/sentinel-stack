/**
 * @file hal_common.h
 * @brief HAL(Hardware Abstraction Layer) 공통 타입 정의
 *
 * 실제 임베디드 MCU(STM32, NXP i.MX RT 등)에서는
 * 이 헤더가 동일하고, hal_*_posix.c → hal_*_stm32.c 로만 교체됩니다.
 *
 * 지원 인터페이스:
 *  - UART / RS-422 (차동 직렬 통신)
 *  - I2C (Inter-Integrated Circuit, 400kHz Fast-mode)
 *  - SPI (Serial Peripheral Interface, Mode 0~3)
 *  - CAN (Controller Area Network, 1Mbps)
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_HAL_COMMON_H
#define SENTINEL_HAL_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── 버전 ──────────────────────────────────────────────────────────────── */
#define HAL_VERSION_MAJOR   1U
#define HAL_VERSION_MINOR   0U

/* ── 상태 코드 ─────────────────────────────────────────────────────────── */
typedef enum {
    HAL_OK            =  0,  /**< 성공 */
    HAL_ERR_PARAM     = -1,  /**< 잘못된 파라미터 */
    HAL_ERR_IO        = -2,  /**< I/O 오류 */
    HAL_ERR_TIMEOUT   = -3,  /**< 타임아웃 */
    HAL_ERR_BUSY      = -4,  /**< 장치 사용 중 */
    HAL_ERR_NACK      = -5,  /**< I2C: 장치 응답 없음(ACK 없음) */
    HAL_ERR_OVERFLOW  = -6,  /**< 버퍼 오버플로우 */
    HAL_ERR_CRC       = -7,  /**< 프레임 CRC 오류 */
    HAL_ERR_ARB_LOST  = -8,  /**< CAN: 버스 중재 손실 */
} hal_status_t;

/* ── 타임아웃 상수 ─────────────────────────────────────────────────────── */
#define HAL_TIMEOUT_INFINITE  0xFFFFFFFFU  /**< 무한 대기 */
#define HAL_TIMEOUT_NONE      0U           /**< 즉시 반환 */

/* ── 유틸리티 매크로 ───────────────────────────────────────────────────── */
#define HAL_CHECK_NULL(ptr)  do { if ((ptr) == NULL) { return HAL_ERR_PARAM; } } while (0)
#define HAL_CHECK_INIT(flag) do { if (!(flag)) { return HAL_ERR_PARAM; } } while (0)

#endif /* SENTINEL_HAL_COMMON_H */
