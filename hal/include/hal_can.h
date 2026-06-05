/**
 * @file hal_can.h
 * @brief CAN 버스 하드웨어 추상화 인터페이스
 *
 * CAN(Controller Area Network): Bosch 개발, 멀티마스터 직렬 버스
 *  - 2선 차동 신호 (CAN_H, CAN_L) — 높은 EMI 내성
 *  - 우선순위 기반 비파괴 버스 중재 (낮은 ID = 높은 우선순위)
 *  - 최대 1Mbps (클래식 CAN), CAN-FD는 최대 8Mbps
 *  - 자동 오류 감지 + 재전송 (비트 스터핑, CRC-15)
 *
 * 항공우주/방산 활용 예:
 *  - 발사체 액추에이터 제어 버스
 *  - 위성 탑재체 모듈 간 통신 (소형 위성)
 *  - 차량/항공기 내 ECU 간 통신 (MIL-STD-1939 = SAE J1939 군용 파생)
 *
 * CAN Frame 구조:
 *  ┌────────────┬─────┬──────────────┬───────────┐
 *  │ ID (11bit) │ DLC │ Data (0~8B)  │ CRC (15b) │
 *  └────────────┴─────┴──────────────┴───────────┘
 *
 * POSIX 시뮬레이션: pipe 기반 버스, ID 우선순위 정렬 지원
 *  실제 구현:
 *    STM32 → HAL_CAN_AddTxMessage / HAL_CAN_GetRxMessage
 *    Linux → SocketCAN (socket(PF_CAN, ...), /dev/can0)
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_HAL_CAN_H
#define SENTINEL_HAL_CAN_H

#include "hal_common.h"

/* ── CAN 파라미터 ──────────────────────────────────────────────────────── */
#define HAL_CAN_MAX_DLC       8U         /**< 클래식 CAN 최대 데이터 길이 */
#define HAL_CAN_BAUD_125K     125000U
#define HAL_CAN_BAUD_250K     250000U
#define HAL_CAN_BAUD_500K     500000U
#define HAL_CAN_BAUD_1M       1000000U   /**< 1Mbps 최대 보레이트 */

#define HAL_CAN_STD_ID_MASK   0x7FFU     /**< 11비트 표준 ID */
#define HAL_CAN_EXT_ID_MASK   0x1FFFFFFFU /**< 29비트 확장 ID */

/* ── ID 우선순위 정의 (발사체 시스템 예시) ─────────────────────────────── */
#define CAN_ID_FLIGHT_CTRL    0x001U  /**< 비행제어 명령 — 최고 우선순위 */
#define CAN_ID_IMU_DATA       0x010U  /**< IMU 센서 데이터 */
#define CAN_ID_GPS_DATA       0x020U  /**< GPS 위치 데이터 */
#define CAN_ID_ACTUATOR_CMD   0x030U  /**< 액추에이터 제어 */
#define CAN_ID_TELEMETRY      0x100U  /**< 텔레메트리 — 낮은 우선순위 */
#define CAN_ID_HEARTBEAT      0x7FFU  /**< 하트비트 — 최저 우선순위 */

/* ── CAN 프레임 구조체 ─────────────────────────────────────────────────── */
typedef struct {
    uint32_t id;                  /**< 11비트 표준 ID 또는 29비트 확장 ID */
    bool     is_extended_id;      /**< true: 29비트 확장 ID */
    bool     is_remote_frame;     /**< true: RTR 프레임 (데이터 요청) */
    uint8_t  dlc;                 /**< Data Length Code (0~8) */
    uint8_t  data[HAL_CAN_MAX_DLC]; /**< 페이로드 */
    uint64_t timestamp_us;        /**< 수신 시각 (마이크로초) */
} hal_can_frame_t;

/* ── CAN 수신 필터 ─────────────────────────────────────────────────────── */
typedef struct {
    uint32_t id;    /**< 필터 ID */
    uint32_t mask;  /**< 마스크 (1=검사, 0=무시) */
} hal_can_filter_t;

#define HAL_CAN_MAX_FILTERS   8U    /**< 최대 수신 필터 수 */
#define HAL_CAN_RX_BUF_DEPTH  64U   /**< 수신 버퍼 프레임 수 */

/* ── CAN 제어 블록 ─────────────────────────────────────────────────────── */
typedef struct {
    uint32_t baud_rate;
    bool     initialized;
    bool     loopback_mode;   /**< 송수신 자기 루프백 (테스트 모드) */

    /* POSIX 시뮬레이션 */
    int  fd_bus[2];   /**< 버스 파이프: [0]=read, [1]=write */

    /* 수신 필터 */
    hal_can_filter_t filters[HAL_CAN_MAX_FILTERS];
    uint32_t         filter_count;

    /* 통계 */
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t error_count;
    uint32_t arbitration_lost;
} hal_can_t;

/* ── API ───────────────────────────────────────────────────────────────── */

/** @brief CAN 컨트롤러 초기화 */
hal_status_t hal_can_init(hal_can_t *can, uint32_t baud_rate, bool loopback);

/**
 * @brief CAN 프레임 전송
 * @note  낮은 ID = 높은 우선순위 (CAN 중재 규칙)
 */
hal_status_t hal_can_write(hal_can_t *can, const hal_can_frame_t *frame,
                            uint32_t timeout_ms);

/** @brief CAN 프레임 수신 */
hal_status_t hal_can_read(hal_can_t *can, hal_can_frame_t *frame,
                           uint32_t timeout_ms);

/** @brief 수신 필터 추가 */
hal_status_t hal_can_add_filter(hal_can_t *can, uint32_t id, uint32_t mask);

/** @brief CAN 해제 */
void hal_can_deinit(hal_can_t *can);

#endif /* SENTINEL_HAL_CAN_H */
