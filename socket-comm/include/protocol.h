/**
 * @file protocol.h
 * @brief sentinel 커스텀 바이너리 프로토콜 정의
 *
 * Link-16 메시지 포맷과 MIL-STD-1553 이중화 구조를 참고하여
 * 설계한 전술 통신용 바이너리 프로토콜입니다.
 *
 * 패킷 구조 (wire format, big-endian):
 * @code
 *  0        1        2        3        4
 *  ├────────┴────────┴────────┴────────┤
 *  │         Magic (0x53454e54)        │  4 bytes  "SENT"
 *  ├────────┬────────┬────────┬────────┤
 *  │ Version│MsgType │   (padding)     │  2 bytes
 *  ├────────┴────────┴────────┴────────┤
 *  │            Sequence Number        │  4 bytes
 *  ├────────┬────────┬────────┬────────┤
 *  │         Timestamp (ms)   ···      │  8 bytes
 *  ├────────┴────────┴────────┴────────┤
 *  │SrcNode │DstNode │   Topic ID      │  4 bytes
 *  ├────────┴────────┴────────┴────────┤
 *  │           Payload Length          │  4 bytes
 *  ├────────┬────────┬────────┬────────┤
 *  │         AES-256-GCM IV (12 bytes) │ 12 bytes
 *  ├────────┴────────┴────────┴────────┤
 *  │       Encrypted Payload  ···      │ payload_len bytes
 *  ├────────┬────────┬────────┬────────┤
 *  │       AES-GCM Auth Tag (16 bytes) │ 16 bytes
 *  ├────────┴────────┴────────┴────────┤
 *  │           CRC-32 Checksum         │  4 bytes
 *  └───────────────────────────────────┘
 *
 *  헤더 크기: 4+2+4+8+4+4+12 = 38 bytes
 *  최소 패킷: 38 + 0(payload) + 16(tag) + 4(crc) = 58 bytes
 * @endcode
 *
 * 보안:
 *  AES-256-GCM 인증 암호화로 기밀성과 무결성을 동시에 보장합니다.
 *  IV(Nonce)는 매 패킷마다 /dev/urandom으로 새로 생성됩니다.
 *  CRC-32는 전송 오류 감지용이며 보안 목적이 아닙니다.
 *
 * @author  Park Sang Min
 * @date    2025
 * @version 1.0
 */

#ifndef SENTINEL_PROTOCOL_H
#define SENTINEL_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* =========================================================================
 * 프로토콜 상수
 * ========================================================================= */

/** 매직 넘버: ASCII "SENT" = 0x53454e54 */
#define SENTINEL_MAGIC           0x53454E54U

/** 현재 프로토콜 버전 */
#define SENTINEL_VERSION         1U

/** TCP 기본 포트 (신뢰 채널) */
#define SENTINEL_PORT_TCP        9000U

/** UDP 기본 포트 (고속 채널) */
#define SENTINEL_PORT_UDP        9001U

/** AES-256-GCM Initialization Vector 크기 */
#define SENTINEL_AES_IV_LEN      12U

/** AES-256 키 크기 (256 비트 = 32 바이트) */
#define SENTINEL_AES_KEY_LEN     32U

/** AES-GCM 인증 태그 크기 */
#define SENTINEL_AES_TAG_LEN     16U

/** 브로드캐스트/멀티캐스트 노드 ID */
#define SENTINEL_NODE_BROADCAST  0xFFU

/** 페이로드 최대 크기 (암호화 전) */
#define SENTINEL_MAX_PAYLOAD     4096U

/** 헤더 크기: magic(4)+ver(1)+type(1)+seq(4)+ts(8)+src(1)+dst(1)+topic(2)+len(4)+iv(12) */
#define SENTINEL_HEADER_SIZE     38U

/** 최소 패킷 크기 (헤더 + GCM 태그 + CRC) */
#define SENTINEL_MIN_PACKET_SIZE (SENTINEL_HEADER_SIZE + SENTINEL_AES_TAG_LEN + 4U)

/** 최대 패킷 크기 (헤더 + 최대 페이로드 + GCM 태그 + CRC) */
#define SENTINEL_MAX_PACKET_SIZE (SENTINEL_HEADER_SIZE + SENTINEL_MAX_PAYLOAD \
                                   + SENTINEL_AES_TAG_LEN + 4U)

/* =========================================================================
 * 메시지 타입
 * ========================================================================= */

/**
 * @brief sentinel 메시지 타입 열거형
 *
 * Link-16 메시지 카테고리를 단순화하여 적용했습니다.
 */
typedef enum {
    SENTINEL_MSG_DATA       = 0x01U, /**< 센서/측정 데이터 */
    SENTINEL_MSG_STATUS     = 0x02U, /**< 시스템 상태 정보 */
    SENTINEL_MSG_CMD        = 0x03U, /**< 명령 (Command) */
    SENTINEL_MSG_HEARTBEAT  = 0x04U, /**< 연결 유지 Heartbeat */
    SENTINEL_MSG_ACK        = 0x05U  /**< 수신 확인 응답 */
} sentinel_msg_type_t;

/* =========================================================================
 * 토픽 ID (Publisher-Subscriber 라우팅 키)
 * ========================================================================= */

/**
 * @brief DDS 토픽 개념을 단순화한 토픽 ID
 */
typedef enum {
    SENTINEL_TOPIC_SENSOR_DATA  = 0x0001U, /**< 센서 데이터 스트림 */
    SENTINEL_TOPIC_STATUS       = 0x0002U, /**< 시스템 상태 */
    SENTINEL_TOPIC_CMD          = 0x0003U, /**< 명령 채널 */
    SENTINEL_TOPIC_HEARTBEAT    = 0x0004U  /**< Heartbeat 채널 */
} sentinel_topic_id_t;

/* =========================================================================
 * 패킷 구조체
 * ========================================================================= */

/**
 * @brief sentinel 패킷 헤더 (네트워크 바이트 순서로 직렬화)
 *
 * packed 속성으로 패딩 없이 연속 배치됩니다.
 * 직접 캐스팅하지 말고 반드시 sentinel_packet_serialize() 를 통해 사용하세요.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                      /**< 0x53454E54 */
    uint8_t  version;                    /**< 프로토콜 버전 */
    uint8_t  msg_type;                   /**< sentinel_msg_type_t */
    uint32_t seq_num;                    /**< 발신 노드 기준 시퀀스 번호 */
    uint64_t timestamp_ms;              /**< 발신 시각 (Unix ms) */
    uint8_t  src_node_id;               /**< 발신 노드 ID */
    uint8_t  dst_node_id;               /**< 수신 노드 ID (0xFF=브로드캐스트) */
    uint16_t topic_id;                  /**< 토픽 ID */
    uint32_t payload_len;               /**< 암호화된 페이로드 길이 */
    uint8_t  aes_iv[SENTINEL_AES_IV_LEN]; /**< AES-256-GCM IV */
} sentinel_header_t;

/**
 * @brief sentinel 전체 패킷 (정적 버퍼 기반)
 *
 * 동적 할당 없이 스택 또는 정적 영역에 선언하세요.
 * enc_payload_len 은 payload_len 과 동일하며 명시적 가독성을 위해 보유합니다.
 */
typedef struct {
    sentinel_header_t header;                        /**< 38바이트 헤더 */
    uint8_t           enc_payload[SENTINEL_MAX_PAYLOAD]; /**< 암호화된 페이로드 */
    uint8_t           aes_tag[SENTINEL_AES_TAG_LEN]; /**< GCM 인증 태그 */
    uint32_t          crc32;                         /**< CRC-32 체크섬 */
    uint32_t          enc_payload_len;               /**< 실제 암호화 페이로드 길이 */
} sentinel_packet_t;

/* =========================================================================
 * 직렬화 / 역직렬화 API
 * ========================================================================= */

/**
 * @brief 패킷을 wire format 바이트 배열로 직렬화
 *
 * 헤더 필드를 big-endian(네트워크 바이트 순서)으로 변환하고,
 * 페이로드, GCM 태그, CRC-32 를 순서대로 버퍼에 기록합니다.
 *
 * CRC-32 계산 범위: 헤더(38) + 암호화 페이로드 + GCM 태그
 *
 * @param pkt      직렬화할 패킷 포인터
 * @param buf      출력 버퍼
 * @param buf_size 출력 버퍼 크기 (SENTINEL_MAX_PACKET_SIZE 이상 권장)
 * @param out_len  [출력] 실제 기록된 바이트 수
 * @return 0 성공, -1 실패 (버퍼 부족, NULL 등)
 */
int sentinel_packet_serialize(const sentinel_packet_t *pkt,
                               uint8_t *buf, size_t buf_size,
                               size_t *out_len);

/**
 * @brief 바이트 배열에서 패킷을 역직렬화
 *
 * 매직 번호, 버전, CRC-32 를 검증한 후 pkt 에 복원합니다.
 *
 * @param buf     입력 바이트 배열
 * @param buf_len 입력 길이
 * @param pkt     [출력] 복원된 패킷
 * @return 0 성공, -1 실패 (매직/CRC 불일치, 길이 부족 등)
 */
int sentinel_packet_deserialize(const uint8_t *buf, size_t buf_len,
                                 sentinel_packet_t *pkt);

/**
 * @brief CRC-32 계산 (IEEE 802.3 다항식 0xEDB88320)
 *
 * @param data 계산할 데이터
 * @param len  데이터 길이
 * @return CRC-32 값
 */
uint32_t sentinel_crc32(const uint8_t *data, size_t len);

/**
 * @brief 메시지 타입을 문자열로 변환
 * @param type 메시지 타입
 * @return 정적 문자열 포인터 (해제 불필요)
 */
const char *sentinel_msg_type_str(sentinel_msg_type_t type);

/**
 * @brief 토픽 ID를 문자열로 변환
 * @param topic 토픽 ID
 * @return 정적 문자열 포인터 (해제 불필요)
 */
const char *sentinel_topic_str(sentinel_topic_id_t topic);

/**
 * @brief 패킷 헤더를 stdout에 출력 (디버깅용)
 * @param pkt 출력할 패킷
 */
void sentinel_packet_print(const sentinel_packet_t *pkt);

#endif /* SENTINEL_PROTOCOL_H */
