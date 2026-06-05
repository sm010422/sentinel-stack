/**
 * @file hal_spi_posix.c
 * @brief SPI HAL POSIX 시뮬레이션 구현
 *
 * 두 쌍의 pipe()로 MOSI/MISO 전이중 채널을 시뮬레이션합니다.
 *
 * 실제 SPI 전송:
 *   - CS(Chip Select) 로우 → 클럭 펄스마다 MOSI 1비트 출력 + MISO 1비트 수신
 *   - CS 하이 → 전송 종료
 *
 * 실제 구현 교체:
 *   STM32: HAL_SPI_TransmitReceive(&hspi1, tx, rx, len, 100)
 *          DMA 모드: HAL_SPI_TransmitReceive_DMA()
 *   Linux: ioctl(fd, SPI_IOC_MESSAGE(1), &spi_transfer)
 */

#define _POSIX_C_SOURCE 200809L

#include "hal_spi.h"
#include <poll.h>
#include <string.h>
#include <unistd.h>

hal_status_t hal_spi_init(hal_spi_t *spi, uint32_t clock_hz,
                           hal_spi_mode_t mode, uint8_t bits_per_word)
{
    HAL_CHECK_NULL(spi);

    if (clock_hz == 0U) { return HAL_ERR_PARAM; }
    if (bits_per_word != 8U && bits_per_word != 16U) { return HAL_ERR_PARAM; }

    memset(spi, 0, sizeof(*spi));

    spi->clock_hz      = clock_hz;
    spi->mode          = mode;
    spi->bit_order     = HAL_SPI_MSB_FIRST;
    spi->bits_per_word = bits_per_word;
    spi->fd_mosi[0]    = -1;
    spi->fd_mosi[1]    = -1;
    spi->fd_miso[0]    = -1;
    spi->fd_miso[1]    = -1;

    if (pipe(spi->fd_mosi) != 0) { return HAL_ERR_IO; }
    if (pipe(spi->fd_miso) != 0) {
        close(spi->fd_mosi[0]);
        close(spi->fd_mosi[1]);
        return HAL_ERR_IO;
    }

    spi->initialized = true;
    return HAL_OK;
}

hal_status_t hal_spi_transfer(hal_spi_t *spi, const uint8_t *tx_buf,
                               uint8_t *rx_buf, size_t len)
{
    HAL_CHECK_NULL(spi);
    HAL_CHECK_INIT(spi->initialized);

    if (len == 0U) { return HAL_ERR_PARAM; }

    /* MOSI 전송: tx_buf가 NULL이면 0xFF (dummy 클럭) */
    if (tx_buf != NULL) {
        if (write(spi->fd_mosi[1], tx_buf, len) < 0) { return HAL_ERR_IO; }
    } else {
        uint8_t dummy[256];
        size_t remaining = len;
        while (remaining > 0U) {
            size_t chunk = (remaining < sizeof(dummy)) ? remaining : sizeof(dummy);
            memset(dummy, 0xFF, chunk);
            if (write(spi->fd_mosi[1], dummy, chunk) < 0) { return HAL_ERR_IO; }
            remaining -= chunk;
        }
    }

    /* 루프백: MOSI에서 읽어서 MISO로 복사 (슬레이브 시뮬레이션) */
    uint8_t loopback[256];
    size_t   to_copy = len;
    while (to_copy > 0U) {
        size_t chunk = (to_copy < sizeof(loopback)) ? to_copy : sizeof(loopback);
        ssize_t n = read(spi->fd_mosi[0], loopback, chunk);
        if (n <= 0) { return HAL_ERR_IO; }
        if (write(spi->fd_miso[1], loopback, (size_t)n) < 0) { return HAL_ERR_IO; }
        to_copy -= (size_t)n;
    }

    /* MISO 수신 */
    if (rx_buf != NULL) {
        size_t total = 0U;
        while (total < len) {
            struct pollfd pfd = { .fd = spi->fd_miso[0], .events = POLLIN };
            if (poll(&pfd, 1, 100) <= 0) { return HAL_ERR_TIMEOUT; }
            ssize_t n = read(spi->fd_miso[0], rx_buf + total, len - total);
            if (n <= 0) { return HAL_ERR_IO; }
            total += (size_t)n;
        }
    }

    spi->transfer_bytes += (uint64_t)len;
    spi->transfer_count++;
    return HAL_OK;
}

hal_status_t hal_spi_write(hal_spi_t *spi, const uint8_t *tx_buf, size_t len)
{
    return hal_spi_transfer(spi, tx_buf, NULL, len);
}

hal_status_t hal_spi_read(hal_spi_t *spi, uint8_t *rx_buf, size_t len)
{
    return hal_spi_transfer(spi, NULL, rx_buf, len);
}

void hal_spi_deinit(hal_spi_t *spi)
{
    if (spi == NULL || !spi->initialized) { return; }

    if (spi->fd_mosi[0] >= 0) { close(spi->fd_mosi[0]); }
    if (spi->fd_mosi[1] >= 0) { close(spi->fd_mosi[1]); }
    if (spi->fd_miso[0] >= 0) { close(spi->fd_miso[0]); }
    if (spi->fd_miso[1] >= 0) { close(spi->fd_miso[1]); }

    memset(spi, 0, sizeof(*spi));
}
