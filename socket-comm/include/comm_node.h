/**
 * @file comm_node.h
 * @brief sentinel 통신 노드 공개 API
 *
 * 전술 네트워크의 단일 노드를 추상화합니다.
 * 노드는 Commander(지휘소), Sensor(센서 플랫폼), Relay(중계 노드) 중
 * 하나의 역할을 수행합니다.
 *
 * 통신 이중화 구조 (MIL-STD-1553 참고):
 *  - TCP 채널: 신뢰성 보장, 연결 유지, 명령·상태 메시지
 *  - UDP 채널: 저지연, 센서 데이터·Heartbeat 전송
 *  - Heartbeat 기반 장애 감지 및 자동 재연결
 *
 * 이벤트 루프:
 *  comm_node_run() → epoll(Linux) / kqueue(macOS) 기반
 *   ├─ TCP 수신 이벤트 → sentinel_packet 역직렬화 → pubsub 발행
 *   ├─ UDP 수신 이벤트 → 동일 처리
 *   ├─ Heartbeat 타이머 → TCP로 HEARTBEAT 패킷 발행
 *   └─ 연결 감시 → 재연결 시도 (< 500ms)
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_COMM_NODE_H
#define SENTINEL_COMM_NODE_H

#include "protocol.h"
#include "crypto.h"
#include "pubsub.h"
#include "io_multiplexer.h"

#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * 노드 파라미터 상수
 * ========================================================================= */

/** 재연결 시도 최대 대기 시간 (ms) */
#define COMM_RECONNECT_MS      500U

/** Heartbeat 전송 주기 (ms) */
#define COMM_HEARTBEAT_MS      1000U

/** Heartbeat 무응답 시 연결 끊김 판단 임계치 (회) */
#define COMM_HB_MISS_THRESHOLD 3U

/** 노드 이름 최대 길이 */
#define COMM_NODE_NAME_LEN     32U

/** 피어 주소 최대 길이 */
#define COMM_ADDR_LEN          64U

/* =========================================================================
 * 노드 역할
 * ========================================================================= */

/**
 * @brief 통신 노드 역할
 */
typedef enum {
    NODE_ROLE_COMMANDER = 0, /**< 지휘소 노드: TCP 서버, 토픽 발행 주체 */
    NODE_ROLE_SENSOR    = 1, /**< 센서 노드: 센서 데이터 생성·발행 */
    NODE_ROLE_RELAY     = 2  /**< 중계 노드: 패킷 포워딩 */
} node_role_t;

/* =========================================================================
 * 통신 노드 구조체
 * ========================================================================= */

/**
 * @brief sentinel 통신 노드 제어 블록
 *
 * 정적으로 선언하여 사용하세요. 동적 할당 불필요합니다.
 */
typedef struct {
    /* ── 식별 정보 ─────────────────────────────────────── */
    uint8_t      node_id;               /**< 노드 고유 ID (1~254, 255=브로드캐스트) */
    char         name[COMM_NODE_NAME_LEN]; /**< 노드 이름 */
    node_role_t  role;                  /**< 노드 역할 */

    /* ── 소켓 ────────────────────────────────────────────── */
    int          tcp_fd;                /**< TCP 소켓 fd (-1이면 미사용) */
    int          udp_fd;                /**< UDP 소켓 fd */
    int          tcp_server_fd;         /**< TCP 서버 listen fd (Commander용) */
    uint32_t     tcp_port;             /**< 자신의 TCP 포트 */
    uint32_t     udp_port;             /**< 자신의 UDP 포트 */

    /* ── 피어 연결 ──────────────────────────────────────── */
    char         peer_addr[COMM_ADDR_LEN]; /**< 연결할 피어 주소 */
    uint32_t     peer_tcp_port;           /**< 피어 TCP 포트 */
    bool         connected;               /**< TCP 연결 상태 */
    uint32_t     hb_miss_count;           /**< Heartbeat 미수신 카운트 */
    uint64_t     last_hb_recv_ms;         /**< 마지막 Heartbeat 수신 시각 */

    /* ── 서브시스템 ──────────────────────────────────────── */
    pubsub_t     pubsub;                /**< Publisher-Subscriber 시스템 */
    crypto_ctx_t crypto;                /**< AES-256-GCM 암호화 컨텍스트 */
    io_ctx_t     io;                    /**< I/O 멀티플렉서 (epoll/kqueue) */

    /* ── 프로토콜 상태 ──────────────────────────────────── */
    uint32_t     seq_num;               /**< 발신 시퀀스 번호 (자동 증가) */

    /* ── 통계 ────────────────────────────────────────────── */
    uint64_t     packets_sent;          /**< 전송 패킷 수 */
    uint64_t     packets_recv;          /**< 수신 패킷 수 */
    uint64_t     bytes_sent;            /**< 전송 바이트 수 */
    uint64_t     bytes_recv;            /**< 수신 바이트 수 */
    uint64_t     reconnect_count;       /**< 재연결 횟수 */
    uint64_t     decrypt_fail_count;    /**< 복호화 실패 횟수 */
    uint64_t     crc_fail_count;        /**< CRC 오류 횟수 */

    /* ── 런타임 제어 ─────────────────────────────────────── */
    bool         running;               /**< 이벤트 루프 실행 중 여부 */
} comm_node_t;

/* =========================================================================
 * API
 * ========================================================================= */

/**
 * @brief 통신 노드 초기화
 *
 * pubsub, crypto, 소켓을 초기화하고 기본 토픽을 등록합니다.
 * 암호화 키는 데모용 하드코딩 키를 사용합니다.
 * 실제 배포 시에는 외부 키 배포 프로토콜(KDP)로 교체해야 합니다.
 *
 * @param node     초기화할 노드
 * @param id       노드 ID (1~254)
 * @param role     노드 역할
 * @param tcp_port TCP 포트 (0이면 기본값 SENTINEL_PORT_TCP)
 * @param udp_port UDP 포트 (0이면 기본값 SENTINEL_PORT_UDP)
 * @return 0 성공, -1 실패
 */
int comm_node_init(comm_node_t *node, uint8_t id, node_role_t role,
                   uint32_t tcp_port, uint32_t udp_port);

/**
 * @brief 피어 TCP 서버에 연결 (Sensor/Relay 노드)
 *
 * @param node      노드
 * @param addr      피어 주소 (IPv4)
 * @param port      피어 TCP 포트
 * @return 0 성공, -1 실패
 */
int comm_node_connect(comm_node_t *node, const char *addr, uint32_t port);

/**
 * @brief TCP 서버 소켓 열기 (Commander 노드)
 *
 * @param node 노드
 * @return 0 성공, -1 실패
 */
int comm_node_listen(comm_node_t *node);

/**
 * @brief 데이터를 암호화하여 패킷으로 발행
 *
 * TCP 채널로 전송합니다. UDP 멀티캐스트가 필요하면 별도 구현하세요.
 * topic_id에 구독한 로컬 콜백도 함께 호출합니다.
 *
 * @param node     발신 노드
 * @param topic_id 발행 토픽
 * @param data     평문 페이로드
 * @param len      페이로드 길이
 * @return 0 성공, -1 실패
 */
int comm_node_publish(comm_node_t *node, uint16_t topic_id,
                      const uint8_t *data, size_t len);

/**
 * @brief 이벤트 루프 진입 (블로킹)
 *
 * epoll / kqueue 기반 이벤트 루프를 실행합니다.
 * comm_node_stop() 또는 SIGINT 수신 시 반환합니다.
 *
 * @param node 노드
 * @return 0 정상 종료, -1 오류
 */
int comm_node_run(comm_node_t *node);

/**
 * @brief 이벤트 루프 종료 요청
 *
 * comm_node_run() 이 다음 이벤트 처리 후 반환합니다.
 *
 * @param node 노드
 */
void comm_node_stop(comm_node_t *node);

/**
 * @brief 노드 종료 및 소켓 해제
 *
 * @param node 노드
 */
void comm_node_shutdown(comm_node_t *node);

/**
 * @brief 노드 통계 출력
 *
 * @param node 노드
 */
void comm_node_print_stats(const comm_node_t *node);

/**
 * @brief 노드 역할을 문자열로 변환
 *
 * @param role 노드 역할
 * @return 역할 이름 문자열
 */
const char *node_role_str(node_role_t role);

#endif /* SENTINEL_COMM_NODE_H */
