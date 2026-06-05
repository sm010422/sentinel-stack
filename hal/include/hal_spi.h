/**
 * @file hal_spi.h
 * @brief SPI 하드웨어 추상화 인터페이스
 *
 * SPI(Serial Peripheral Interface): Motorola 개발, 4선 전이중 직렬 버스
 *  - SCLK: 클럭   MOSI: 마스터→슬레이브   MISO: 슬레이브→마스터   CS: 칩 선택
 *  - I2C보다 빠름: 50MHz 이상 가능
 *  - CPOL(클럭 극성) × CPHA(클럭 위상) → Mode 0/1/2/3
 *
 * 항공우주 활용 예:
 *  - ICM-42688-P (고정밀 IMU, SpaceX Falcon 9 계열) → SPI Mode 0, 24MHz
 *  - ADXL375 (고g 가속도계, 발사체 충격 측정) → SPI Mode 3
 *  - W25Q128 (SPI NOR Flash, 파라미터/펌웨어 저장)
 *
 * POSIX 시뮬레이션: 두 쌍의 pipe()로 MOSI/MISO 채널 구현
 *  실제 구현:
 *    STM32 → HAL_SPI_TransmitReceive (DMA 모드)
 *    Linux → /dev/spidev*, ioctl(SPI_IOC_MESSAGE)
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_HAL_SPI_H
#define SENTINEL_HAL_SPI_H

#include "hal_common.h"

/* ── SPI 클럭 속도 상수 ────────────────────────────────────────────────── */
#define HAL_SPI_CLOCK_1MHZ    1000000U
#define HAL_SPI_CLOCK_8MHZ    8000000U
#define HAL_SPI_CLOCK_24MHZ  24000000U
#define HAL_SPI_CLOCK_48MHZ  48000000U

/* ── SPI 모드 (CPOL/CPHA 조합) ─────────────────────────────────────────── */
typedef enum {
    HAL_SPI_MODE_0 = 0,  /**< CPOL=0, CPHA=0 — 유휴 Low, 상승 에지 샘플링 */
    HAL_SPI_MODE_1 = 1,  /**< CPOL=0, CPHA=1 — 유휴 Low, 하강 에지 샘플링 */
    HAL_SPI_MODE_2 = 2,  /**< CPOL=1, CPHA=0 — 유휴 High, 하강 에지 샘플링 */
    HAL_SPI_MODE_3 = 3,  /**< CPOL=1, CPHA=1 — 유휴 High, 상승 에지 샘플링 */
} hal_spi_mode_t;

/* ── 비트 순서 ─────────────────────────────────────────────────────────── */
typedef enum {
    HAL_SPI_MSB_FIRST = 0,  /**< 최상위 비트 먼저 (대부분의 센서) */
    HAL_SPI_LSB_FIRST = 1,  /**< 최하위 비트 먼저 */
} hal_spi_bitorder_t;

/* ── SPI 제어 블록 ─────────────────────────────────────────────────────── */
typedef struct {
    uint32_t           clock_hz;
    hal_spi_mode_t     mode;
    hal_spi_bitorder_t bit_order;
    uint8_t            bits_per_word;  /**< 8 또는 16 비트 */

    /* POSIX 시뮬레이션: pipe 파일 디스크립터
     * fd_mosi[0]=read fd_mosi[1]=write → MOSI 채널
     * fd_miso[0]=read fd_miso[1]=write → MISO 채널
     */
    int  fd_mosi[2];
    int  fd_miso[2];
    bool initialized;

    /* 성능 카운터 */
    uint64_t transfer_bytes;
    uint32_t transfer_count;
} hal_spi_t;

/* ── API ───────────────────────────────────────────────────────────────── */

/** @brief SPI 마스터 초기화 */
hal_status_t hal_spi_init(hal_spi_t *spi, uint32_t clock_hz,
                           hal_spi_mode_t mode, uint8_t bits_per_word);

/**
 * @brief 전이중 전송 (동시 TX/RX)
 * @param tx_buf  송신 데이터 (NULL이면 0xFF 전송)
 * @param rx_buf  수신 데이터 저장 (NULL이면 수신 무시)
 * @param len     전송 바이트 수
 */
hal_status_t hal_spi_transfer(hal_spi_t *spi, const uint8_t *tx_buf,
                               uint8_t *rx_buf, size_t len);

/** @brief 송신 전용 (Rx 무시) */
hal_status_t hal_spi_write(hal_spi_t *spi, const uint8_t *tx_buf, size_t len);

/** @brief 수신 전용 (0xFF 전송) */
hal_status_t hal_spi_read(hal_spi_t *spi, uint8_t *rx_buf, size_t len);

/** @brief SPI 해제 */
void hal_spi_deinit(hal_spi_t *spi);

#endif /* SENTINEL_HAL_SPI_H */
