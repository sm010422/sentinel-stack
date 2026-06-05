/**
 * @file hal_i2c_posix.c
 * @brief I2C HAL POSIX 시뮬레이션 구현
 *
 * 레지스터 파일 방식으로 I2C 버스를 시뮬레이션합니다.
 * 실제 I2C 프로토콜:
 *   START → 7비트 주소 + R/W 비트 → ACK → 레지스터 주소
 *   → ACK → 데이터 → ACK/NACK → STOP
 *
 * 실제 구현 교체:
 *   STM32: HAL_I2C_Mem_Write(&hi2c1, dev_addr<<1, reg, I2C_MEMADD_SIZE_8BIT, data, len, 100)
 *   Linux: open("/dev/i2c-1"), ioctl(fd, I2C_SLAVE, dev_addr), write/read()
 */

#include "hal_i2c.h"
#include <string.h>

hal_status_t hal_i2c_init(hal_i2c_t *i2c, uint32_t clock_hz)
{
    HAL_CHECK_NULL(i2c);

    if (clock_hz == 0U) { return HAL_ERR_PARAM; }

    memset(i2c, 0, sizeof(*i2c));
    i2c->clock_hz    = clock_hz;
    i2c->initialized = true;
    return HAL_OK;
}

hal_status_t hal_i2c_register_device(hal_i2c_t *i2c, uint8_t dev_addr)
{
    HAL_CHECK_NULL(i2c);
    HAL_CHECK_INIT(i2c->initialized);

    if (dev_addr >= HAL_I2C_MAX_DEVICES) { return HAL_ERR_PARAM; }

    i2c->device_present[dev_addr] = true;
    memset(i2c->regs[dev_addr], 0, HAL_I2C_REG_SIZE);
    return HAL_OK;
}

hal_status_t hal_i2c_write_reg(hal_i2c_t *i2c, uint8_t dev_addr,
                                uint8_t reg_addr,
                                const uint8_t *data, size_t len)
{
    HAL_CHECK_NULL(i2c);
    HAL_CHECK_NULL(data);
    HAL_CHECK_INIT(i2c->initialized);

    if (dev_addr >= HAL_I2C_MAX_DEVICES) { return HAL_ERR_PARAM; }
    if (!i2c->device_present[dev_addr])  { i2c->nack_count++; return HAL_ERR_NACK; }
    if (len == 0U || len > HAL_I2C_MAX_TRANSFER) { return HAL_ERR_PARAM; }

    /* 레지스터 경계 초과 시 순환 */
    for (size_t i = 0U; i < len; i++) {
        uint8_t addr = (uint8_t)((reg_addr + i) & 0xFFU);
        i2c->regs[dev_addr][addr] = data[i];
    }

    i2c->transaction_count++;
    return HAL_OK;
}

hal_status_t hal_i2c_read_reg(hal_i2c_t *i2c, uint8_t dev_addr,
                               uint8_t reg_addr,
                               uint8_t *data, size_t len)
{
    HAL_CHECK_NULL(i2c);
    HAL_CHECK_NULL(data);
    HAL_CHECK_INIT(i2c->initialized);

    if (dev_addr >= HAL_I2C_MAX_DEVICES) { return HAL_ERR_PARAM; }
    if (!i2c->device_present[dev_addr])  { i2c->nack_count++; return HAL_ERR_NACK; }
    if (len == 0U || len > HAL_I2C_MAX_TRANSFER) { return HAL_ERR_PARAM; }

    for (size_t i = 0U; i < len; i++) {
        uint8_t addr = (uint8_t)((reg_addr + i) & 0xFFU);
        data[i] = i2c->regs[dev_addr][addr];
    }

    i2c->transaction_count++;
    return HAL_OK;
}

hal_status_t hal_i2c_probe(const hal_i2c_t *i2c, uint8_t dev_addr)
{
    HAL_CHECK_NULL(i2c);
    HAL_CHECK_INIT(i2c->initialized);

    if (dev_addr >= HAL_I2C_MAX_DEVICES) { return HAL_ERR_PARAM; }
    return i2c->device_present[dev_addr] ? HAL_OK : HAL_ERR_NACK;
}

void hal_i2c_deinit(hal_i2c_t *i2c)
{
    if (i2c == NULL) { return; }
    memset(i2c, 0, sizeof(*i2c));
}
