/**
 * @file hal_can_posix.c
 * @brief CAN HAL POSIX 시뮬레이션 구현
 *
 * pipe 기반으로 CAN 버스 프레임 전송을 시뮬레이션합니다.
 * 프레임을 직렬화하여 pipe에 write/read합니다.
 *
 * 실제 구현 교체:
 *   STM32: HAL_CAN_AddTxMessage(&hcan1, &tx_header, data, &tx_mailbox)
 *          HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rx_header, data)
 *   Linux (SocketCAN):
 *     int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
 *     struct can_frame frame = { .can_id = id, .can_dlc = dlc };
 *     write(s, &frame, sizeof(frame));
 */

#define _POSIX_C_SOURCE 200809L

#include "hal_can.h"
#include <poll.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* pipe로 직렬화할 내부 프레임 구조 */
typedef struct {
    uint32_t id;
    uint8_t  flags;   /* bit0: extended_id, bit1: remote_frame */
    uint8_t  dlc;
    uint8_t  data[HAL_CAN_MAX_DLC];
    uint64_t timestamp_us;
} can_wire_frame_t;

static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000U + (uint64_t)ts.tv_nsec / 1000U;
}

hal_status_t hal_can_init(hal_can_t *can, uint32_t baud_rate, bool loopback)
{
    HAL_CHECK_NULL(can);

    if (baud_rate == 0U) { return HAL_ERR_PARAM; }

    memset(can, 0, sizeof(*can));
    can->baud_rate     = baud_rate;
    can->loopback_mode = loopback;
    can->fd_bus[0]     = -1;
    can->fd_bus[1]     = -1;

    if (pipe(can->fd_bus) != 0) { return HAL_ERR_IO; }

    can->initialized = true;
    return HAL_OK;
}

hal_status_t hal_can_write(hal_can_t *can, const hal_can_frame_t *frame,
                            uint32_t timeout_ms)
{
    HAL_CHECK_NULL(can);
    HAL_CHECK_NULL(frame);
    HAL_CHECK_INIT(can->initialized);

    if (frame->dlc > HAL_CAN_MAX_DLC) { return HAL_ERR_PARAM; }
    if ((frame->id & ~HAL_CAN_EXT_ID_MASK) != 0U) { return HAL_ERR_PARAM; }

    (void)timeout_ms;

    can_wire_frame_t wf;
    wf.id           = frame->id;
    wf.flags        = (frame->is_extended_id  ? 0x01U : 0x00U) |
                      (frame->is_remote_frame ? 0x02U : 0x00U);
    wf.dlc          = frame->dlc;
    wf.timestamp_us = get_time_us();
    memcpy(wf.data, frame->data, HAL_CAN_MAX_DLC);

    if (write(can->fd_bus[1], &wf, sizeof(wf)) != (ssize_t)sizeof(wf)) {
        can->error_count++;
        return HAL_ERR_IO;
    }

    can->tx_count++;
    return HAL_OK;
}

hal_status_t hal_can_read(hal_can_t *can, hal_can_frame_t *frame,
                           uint32_t timeout_ms)
{
    HAL_CHECK_NULL(can);
    HAL_CHECK_NULL(frame);
    HAL_CHECK_INIT(can->initialized);

    struct pollfd pfd = { .fd = can->fd_bus[0], .events = POLLIN };
    int timeout = (timeout_ms == HAL_TIMEOUT_INFINITE) ? -1 : (int)timeout_ms;

    if (poll(&pfd, 1, timeout) <= 0) { return HAL_ERR_TIMEOUT; }

    can_wire_frame_t wf;
    if (read(can->fd_bus[0], &wf, sizeof(wf)) != (ssize_t)sizeof(wf)) {
        return HAL_ERR_IO;
    }

    /* 수신 필터 검사 */
    bool accepted = (can->filter_count == 0U);
    for (uint32_t i = 0U; i < can->filter_count; i++) {
        if ((wf.id & can->filters[i].mask) ==
            (can->filters[i].id & can->filters[i].mask)) {
            accepted = true;
            break;
        }
    }
    if (!accepted) { return HAL_ERR_PARAM; }

    frame->id              = wf.id;
    frame->is_extended_id  = (wf.flags & 0x01U) != 0U;
    frame->is_remote_frame = (wf.flags & 0x02U) != 0U;
    frame->dlc             = wf.dlc;
    frame->timestamp_us    = wf.timestamp_us;
    memcpy(frame->data, wf.data, HAL_CAN_MAX_DLC);

    can->rx_count++;
    return HAL_OK;
}

hal_status_t hal_can_add_filter(hal_can_t *can, uint32_t id, uint32_t mask)
{
    HAL_CHECK_NULL(can);
    HAL_CHECK_INIT(can->initialized);

    if (can->filter_count >= HAL_CAN_MAX_FILTERS) { return HAL_ERR_OVERFLOW; }

    can->filters[can->filter_count].id   = id;
    can->filters[can->filter_count].mask = mask;
    can->filter_count++;
    return HAL_OK;
}

void hal_can_deinit(hal_can_t *can)
{
    if (can == NULL || !can->initialized) { return; }
    if (can->fd_bus[0] >= 0) { close(can->fd_bus[0]); }
    if (can->fd_bus[1] >= 0) { close(can->fd_bus[1]); }
    memset(can, 0, sizeof(*can));
}
