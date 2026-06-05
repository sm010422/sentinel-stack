/**
 * @file hal_uart.h
 * @brief UART / RS-422 하드웨어 추상화 인터페이스
 *
 * UART: 비동기 직렬 통신 (TTL 레벨, 싱글엔드)
 * RS-422: 차동 신호 방식 UART — EMI 강인성, 최대 1.2km 전송 거리, 10Mbps
 *          방산/우주 탑재체에서 RS-232 대신 널리 사용 (하니웰, BAE Systems 등)
 *
 * POSIX 시뮬레이션: pipe() 쌍으로 TX/RX 채널을 표현
 * 실제 구현:
 *   STM32 → hal_uart_stm32.c  (HAL_UART_Transmit/Receive)
 *   Linux  → hal_uart_linux.c (termios, /dev/ttyS*)
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_HAL_UART_H
#define SENTINEL_HAL_UART_H

#include "hal_common.h"

/* ── 표준 보레이트 상수 ────────────────────────────────────────────────── */
#define HAL_UART_BAUD_9600      9600U
#define HAL_UART_BAUD_38400    38400U
#define HAL_UART_BAUD_115200  115200U
#define HAL_UART_BAUD_460800  460800U
/* RS-422 고속 모드 */
#define HAL_UART_BAUD_1M      1000000U
#define HAL_UART_BAUD_2M      2000000U

/* ── 최대 단일 전송 크기 ───────────────────────────────────────────────── */
#define HAL_UART_TX_BUF_SIZE  4096U
#define HAL_UART_RX_BUF_SIZE  4096U

/* ── 패리티 열거형 ─────────────────────────────────────────────────────── */
typedef enum {
    HAL_UART_PARITY_NONE  = 0,
    HAL_UART_PARITY_ODD   = 1,
    HAL_UART_PARITY_EVEN  = 2,
} hal_uart_parity_t;

/* ── 이중화 모드 열거형 ────────────────────────────────────────────────── */
typedef enum {
    HAL_UART_HALF_DUPLEX  = 0,  /**< 반이중: 한 방향씩 통신 (예: RS-485) */
    HAL_UART_FULL_DUPLEX  = 1,  /**< 전이중: 동시 양방향 (UART, RS-422) */
} hal_uart_duplex_t;

/* ── UART 제어 블록 ────────────────────────────────────────────────────── */
typedef struct {
    /* 물리 설정 */
    uint32_t           baud_rate;
    uint8_t            data_bits;   /**< 데이터 비트: 7 또는 8 */
    uint8_t            stop_bits;   /**< 정지 비트: 1 또는 2 */
    hal_uart_parity_t  parity;
    hal_uart_duplex_t  duplex;
    bool               rs422_mode;  /**< RS-422 차동 신호 시뮬레이션 여부 */

    /* POSIX 시뮬레이션: pipe 파일 디스크립터
     * fd_rx[0]=read  fd_rx[1]=write  → 수신 채널
     * fd_tx[0]=read  fd_tx[1]=write  → 송신 채널
     */
    int  fd_rx[2];
    int  fd_tx[2];
    bool initialized;

    /* 성능 카운터 */
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint32_t frame_errors;  /**< 패리티/프레이밍 오류 수 */
} hal_uart_t;

/* ── API ───────────────────────────────────────────────────────────────── */

/**
 * @brief UART 초기화
 * @param rs422_mode true: RS-422 모드 (차동 신호, 긴 거리, 높은 속도)
 */
hal_status_t hal_uart_init(hal_uart_t *uart, uint32_t baud_rate,
                            uint8_t data_bits, uint8_t stop_bits,
                            hal_uart_parity_t parity, bool rs422_mode);

/** @brief UART 데이터 전송 */
hal_status_t hal_uart_write(hal_uart_t *uart, const uint8_t *data, size_t len,
                             uint32_t timeout_ms);

/** @brief UART 데이터 수신 */
hal_status_t hal_uart_read(hal_uart_t *uart, uint8_t *data, size_t len,
                            size_t *received, uint32_t timeout_ms);

/** @brief 수신 버퍼 플러시 */
hal_status_t hal_uart_flush(hal_uart_t *uart);

/** @brief 통계 조회 */
hal_status_t hal_uart_get_stats(const hal_uart_t *uart,
                                 uint64_t *tx_bytes, uint64_t *rx_bytes,
                                 uint32_t *errors);

/** @brief UART 해제 */
void hal_uart_deinit(hal_uart_t *uart);

#endif /* SENTINEL_HAL_UART_H */
