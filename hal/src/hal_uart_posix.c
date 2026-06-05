/**
 * @file hal_uart_posix.c
 * @brief UART HAL POSIX 시뮬레이션 구현
 *
 * pipe()로 UART TX/RX 채널을 구현합니다.
 * RS-422 모드는 물리 레이어만 다르고 프레임 구조는 동일하므로
 * 동일한 구현을 사용하고 rs422_mode 플래그만 표시합니다.
 *
 * 실제 임베디드 환경 교체:
 *   STM32: HAL_UART_Init() + HAL_UART_Transmit/Receive_DMA()
 *   Linux: open("/dev/ttyS0"), tcsetattr(), write/read()
 */

#define _POSIX_C_SOURCE 200809L

#include "hal_uart.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

hal_status_t hal_uart_init(hal_uart_t *uart, uint32_t baud_rate,
                            uint8_t data_bits, uint8_t stop_bits,
                            hal_uart_parity_t parity, bool rs422_mode)
{
    HAL_CHECK_NULL(uart);

    if ((data_bits != 7U && data_bits != 8U) ||
        (stop_bits != 1U && stop_bits != 2U)) {
        return HAL_ERR_PARAM;
    }

    memset(uart, 0, sizeof(*uart));

    uart->baud_rate  = baud_rate;
    uart->data_bits  = data_bits;
    uart->stop_bits  = stop_bits;
    uart->parity     = parity;
    uart->duplex     = HAL_UART_FULL_DUPLEX;
    uart->rs422_mode = rs422_mode;
    uart->fd_rx[0]   = -1;
    uart->fd_rx[1]   = -1;
    uart->fd_tx[0]   = -1;
    uart->fd_tx[1]   = -1;

    if (pipe(uart->fd_rx) != 0) {
        return HAL_ERR_IO;
    }
    if (pipe(uart->fd_tx) != 0) {
        close(uart->fd_rx[0]);
        close(uart->fd_rx[1]);
        return HAL_ERR_IO;
    }

    uart->initialized = true;
    return HAL_OK;
}

hal_status_t hal_uart_write(hal_uart_t *uart, const uint8_t *data, size_t len,
                             uint32_t timeout_ms)
{
    HAL_CHECK_NULL(uart);
    HAL_CHECK_NULL(data);
    HAL_CHECK_INIT(uart->initialized);

    if (len == 0U || len > HAL_UART_TX_BUF_SIZE) {
        return HAL_ERR_PARAM;
    }

    /* 타임아웃 설정: O_NONBLOCK 방식 */
    (void)timeout_ms;  /* 시뮬레이션에서 pipe는 즉시 반환 */

    ssize_t written = write(uart->fd_tx[1], data, len);
    if (written < 0) {
        uart->frame_errors++;
        return HAL_ERR_IO;
    }

    uart->tx_bytes += (uint64_t)written;
    return HAL_OK;
}

hal_status_t hal_uart_read(hal_uart_t *uart, uint8_t *data, size_t len,
                            size_t *received, uint32_t timeout_ms)
{
    HAL_CHECK_NULL(uart);
    HAL_CHECK_NULL(data);
    HAL_CHECK_NULL(received);
    HAL_CHECK_INIT(uart->initialized);

    *received = 0U;

    struct pollfd pfd = {
        .fd      = uart->fd_rx[0],
        .events  = POLLIN,
        .revents = 0,
    };

    int timeout = (timeout_ms == HAL_TIMEOUT_INFINITE) ? -1 : (int)timeout_ms;
    int ret = poll(&pfd, 1, timeout);

    if (ret < 0)  { return HAL_ERR_IO; }
    if (ret == 0) { return HAL_ERR_TIMEOUT; }

    ssize_t n = read(uart->fd_rx[0], data, len);
    if (n < 0) { return HAL_ERR_IO; }

    *received = (size_t)n;
    uart->rx_bytes += (uint64_t)n;
    return HAL_OK;
}

hal_status_t hal_uart_flush(hal_uart_t *uart)
{
    HAL_CHECK_NULL(uart);
    HAL_CHECK_INIT(uart->initialized);

    uint8_t discard[64];
    while (1) {
        struct pollfd pfd = { .fd = uart->fd_rx[0], .events = POLLIN };
        if (poll(&pfd, 1, 0) <= 0) { break; }
        if (read(uart->fd_rx[0], discard, sizeof(discard)) <= 0) { break; }
    }
    return HAL_OK;
}

hal_status_t hal_uart_get_stats(const hal_uart_t *uart,
                                 uint64_t *tx_bytes, uint64_t *rx_bytes,
                                 uint32_t *errors)
{
    HAL_CHECK_NULL(uart);
    if (tx_bytes) { *tx_bytes = uart->tx_bytes; }
    if (rx_bytes) { *rx_bytes = uart->rx_bytes; }
    if (errors)   { *errors   = uart->frame_errors; }
    return HAL_OK;
}

void hal_uart_deinit(hal_uart_t *uart)
{
    if (uart == NULL || !uart->initialized) { return; }

    if (uart->fd_rx[0] >= 0) { close(uart->fd_rx[0]); }
    if (uart->fd_rx[1] >= 0) { close(uart->fd_rx[1]); }
    if (uart->fd_tx[0] >= 0) { close(uart->fd_tx[0]); }
    if (uart->fd_tx[1] >= 0) { close(uart->fd_tx[1]); }

    memset(uart, 0, sizeof(*uart));
}
