/**
 * @file test_hal_loopback.c
 * @brief HAL 인터페이스 루프백 단위 테스트
 *
 * 각 인터페이스(UART, I2C, SPI, CAN)의 송수신 정합성을 검증합니다.
 * 실제 하드웨어 없이 POSIX 시뮬레이션으로 동일한 테스트가 가능합니다.
 */

#include "hal_uart.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_can.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FAIL_IF(cond, msg) \
    do { if (cond) { fprintf(stderr, "[FAIL] %s\n", (msg)); exit(1); } } while (0)

/* ── UART / RS-422 루프백 테스트 ────────────────────────────────────────── */
static void test_uart_loopback(void)
{
    printf("[TEST] UART 루프백 (115200 baud, 8N1) ...\n");

    hal_uart_t uart;
    FAIL_IF(hal_uart_init(&uart, HAL_UART_BAUD_115200,
                          8, 1, HAL_UART_PARITY_NONE, false) != HAL_OK,
            "UART init 실패");

    const uint8_t tx[] = { 0xAAU, 0x55U, 0x01U, 0x02U, 0x03U };

    FAIL_IF(hal_uart_write(&uart, tx, sizeof(tx), 100) != HAL_OK,
            "UART write 실패");

    /* TX 파이프 read end → RX 파이프 write end 로 수동 루프백 */
    uint8_t tmp[sizeof(tx)];
    ssize_t n = read(uart.fd_tx[0], tmp, sizeof(tmp));
    FAIL_IF(n != (ssize_t)sizeof(tx), "TX pipe read 실패");
    FAIL_IF(write(uart.fd_rx[1], tmp, (size_t)n) != n, "RX pipe write 실패");

    /* RX 수신 */
    uint8_t rx[sizeof(tx)];
    size_t  received = 0U;
    FAIL_IF(hal_uart_read(&uart, rx, sizeof(rx), &received, 200) != HAL_OK,
            "UART read 실패");
    FAIL_IF(received != sizeof(tx),       "수신 길이 불일치");
    FAIL_IF(memcmp(tx, rx, sizeof(tx)), "수신 데이터 불일치");

    hal_uart_deinit(&uart);
    printf("[PASS] UART 루프백\n");
}

static void test_rs422_mode(void)
{
    printf("[TEST] RS-422 모드 초기화 (1Mbps) ...\n");

    hal_uart_t uart;
    FAIL_IF(hal_uart_init(&uart, HAL_UART_BAUD_1M,
                          8, 1, HAL_UART_PARITY_NONE, true) != HAL_OK,
            "RS-422 init 실패");
    FAIL_IF(!uart.rs422_mode,             "RS-422 플래그 미설정");
    FAIL_IF(uart.baud_rate != HAL_UART_BAUD_1M, "보레이트 불일치");

    hal_uart_deinit(&uart);
    printf("[PASS] RS-422 모드\n");
}

/* ── I2C 레지스터 읽기/쓰기 테스트 ─────────────────────────────────────── */
static void test_i2c_register_rw(void)
{
    printf("[TEST] I2C 레지스터 읽기/쓰기 (MPU-6050 시뮬레이션) ...\n");

    hal_i2c_t i2c;
    FAIL_IF(hal_i2c_init(&i2c, HAL_I2C_CLOCK_FAST) != HAL_OK,
            "I2C init 실패");

    /* MPU-6050 I2C 주소: 0x68 */
    const uint8_t DEV_ADDR = 0x68U;
    FAIL_IF(hal_i2c_register_device(&i2c, DEV_ADDR) != HAL_OK,
            "장치 등록 실패");
    FAIL_IF(hal_i2c_probe(&i2c, DEV_ADDR) != HAL_OK, "장치 probe 실패");

    /* WHO_AM_I 레지스터(0x75)에 0x68 쓰기 */
    const uint8_t expected = 0x68U;
    FAIL_IF(hal_i2c_write_reg(&i2c, DEV_ADDR, 0x75U, &expected, 1) != HAL_OK,
            "레지스터 쓰기 실패");

    uint8_t readback = 0U;
    FAIL_IF(hal_i2c_read_reg(&i2c, DEV_ADDR, 0x75U, &readback, 1) != HAL_OK,
            "레지스터 읽기 실패");
    FAIL_IF(readback != expected, "레지스터 값 불일치");

    /* 미등록 장치 NACK 확인 */
    FAIL_IF(hal_i2c_probe(&i2c, 0x42U)      != HAL_ERR_NACK, "NACK 미발생(probe)");
    FAIL_IF(hal_i2c_write_reg(&i2c, 0x42U, 0x00U,
                               &expected, 1) != HAL_ERR_NACK, "NACK 미발생(write)");

    hal_i2c_deinit(&i2c);
    printf("[PASS] I2C 레지스터 읽기/쓰기\n");
}

/* ── SPI 전이중 전송 테스트 ─────────────────────────────────────────────── */
static void test_spi_transfer(void)
{
    printf("[TEST] SPI 전이중 전송 (Mode 0, 8MHz) ...\n");

    hal_spi_t spi;
    FAIL_IF(hal_spi_init(&spi, HAL_SPI_CLOCK_8MHZ, HAL_SPI_MODE_0, 8) != HAL_OK,
            "SPI init 실패");

    const uint8_t tx[] = { 0x80U, 0x3BU, 0x00U, 0x00U };
    uint8_t       rx[sizeof(tx)];

    FAIL_IF(hal_spi_transfer(&spi, tx, rx, sizeof(tx)) != HAL_OK,
            "SPI transfer 실패");
    /* 루프백이므로 rx == tx */
    FAIL_IF(memcmp(tx, rx, sizeof(tx)), "SPI 수신 데이터 불일치");
    FAIL_IF(spi.transfer_count != 1U,   "transfer_count 오류");
    FAIL_IF(spi.transfer_bytes != sizeof(tx), "transfer_bytes 오류");

    hal_spi_deinit(&spi);
    printf("[PASS] SPI 전이중 전송\n");
}

/* ── CAN 프레임 송수신 테스트 ───────────────────────────────────────────── */
static void test_can_frame(void)
{
    printf("[TEST] CAN 프레임 송수신 (1Mbps) ...\n");

    hal_can_t can;
    FAIL_IF(hal_can_init(&can, HAL_CAN_BAUD_1M, true) != HAL_OK,
            "CAN init 실패");

    hal_can_frame_t tx_frame = {
        .id             = CAN_ID_IMU_DATA,
        .is_extended_id = false,
        .dlc            = 8U,
        .data           = { 0x01U, 0x02U, 0x03U, 0x04U,
                            0x05U, 0x06U, 0x07U, 0x08U },
    };
    FAIL_IF(hal_can_write(&can, &tx_frame, 100) != HAL_OK,
            "CAN write 실패");

    hal_can_frame_t rx_frame;
    FAIL_IF(hal_can_read(&can, &rx_frame, 100) != HAL_OK, "CAN read 실패");
    FAIL_IF(rx_frame.id  != CAN_ID_IMU_DATA,  "CAN ID 불일치");
    FAIL_IF(rx_frame.dlc != 8U,               "CAN DLC 불일치");
    FAIL_IF(memcmp(tx_frame.data, rx_frame.data, 8), "CAN 데이터 불일치");

    hal_can_deinit(&can);
    printf("[PASS] CAN 프레임 송수신\n");
}

int main(void)
{
    printf("════════════════════════════════════════\n");
    printf("  HAL 루프백 단위 테스트\n");
    printf("  UART / RS-422 / I2C / SPI / CAN\n");
    printf("════════════════════════════════════════\n\n");

    test_uart_loopback();
    test_rs422_mode();
    test_i2c_register_rw();
    test_spi_transfer();
    test_can_frame();

    printf("\n════════════════════════════════════════\n");
    printf("  모든 HAL 테스트 통과\n");
    printf("════════════════════════════════════════\n");
    return 0;
}
