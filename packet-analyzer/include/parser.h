/**
 * @file parser.h
 * @brief 다계층 프로토콜 파서 인터페이스
 *
 * Ethernet / IP / TCP / UDP / sentinel 커스텀 프로토콜을
 * 계층별로 파싱하는 함수들을 정의합니다.
 *
 * sentinel 커스텀 프로토콜 식별 조건:
 *  - TCP 포트 9000 또는 UDP 포트 9001
 *  - 페이로드 첫 4바이트 == 0x53454E54 ("SENT")
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_PARSER_H
#define SENTINEL_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

/* =========================================================================
 * TCP 플래그 비트
 * ========================================================================= */
#define TCP_FLAG_FIN  0x01U
#define TCP_FLAG_SYN  0x02U
#define TCP_FLAG_RST  0x04U
#define TCP_FLAG_PSH  0x08U
#define TCP_FLAG_ACK  0x10U
#define TCP_FLAG_URG  0x20U

/* =========================================================================
 * 파싱된 각 계층 헤더 구조체
 * ========================================================================= */

typedef struct {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ether_type;  /* 0x0800=IPv4, 0x8100=VLAN */
    uint16_t vlan_id;     /* VLAN 태그가 있을 경우 */
} eth_frame_t;

typedef struct {
    uint8_t  version;
    uint8_t  ihl;
    uint8_t  ttl;
    uint8_t  protocol;   /* 6=TCP, 17=UDP */
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t total_len;
    uint16_t id;
    uint8_t  flags;
} ip_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  flags;
    uint16_t window;
    uint8_t  data_offset; /* 헤더 크기 (바이트) */
} tcp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
} udp_header_t;

typedef struct {
    uint32_t magic;
    uint8_t  version;
    uint8_t  msg_type;
    uint32_t seq_num;
    uint64_t timestamp_ms;
    uint8_t  src_node_id;
    uint8_t  dst_node_id;
    uint16_t topic_id;
    uint32_t payload_len;
    bool     magic_valid;
    bool     crc_valid;
} parsed_sentinel_t;

/**
 * @brief 하나의 캡처 패킷에 대한 전체 파싱 결과
 */
typedef struct {
    struct timeval    timestamp;

    eth_frame_t       eth;
    bool              has_eth;

    ip_header_t       ip;
    bool              has_ip;

    bool              has_tcp;
    bool              has_udp;
    bool              has_sentinel;

    tcp_header_t      tcp;
    udp_header_t      udp;
    parsed_sentinel_t sentinel;

    uint32_t          raw_len;
    uint32_t          payload_offset; /* raw 버퍼 내 transport 페이로드 시작 */
    uint32_t          payload_len;
} parsed_packet_t;

/* =========================================================================
 * 파서 API
 * ========================================================================= */

int  parse_ethernet(const uint8_t *data, size_t len, eth_frame_t *out);
int  parse_ip(const uint8_t *data, size_t len, ip_header_t *out, size_t *ip_hdr_len);
int  parse_tcp(const uint8_t *data, size_t len, tcp_header_t *out, size_t *tcp_hdr_len);
int  parse_udp(const uint8_t *data, size_t len, udp_header_t *out);
int  parse_sentinel(const uint8_t *data, size_t len, parsed_sentinel_t *out);
int  parse_packet(const uint8_t *data, size_t len, parsed_packet_t *out);

void ip_to_str(uint32_t ip, char *buf, size_t buf_len);
void mac_to_str(const uint8_t *mac, char *buf, size_t buf_len);
const char *sentinel_msg_type_name(uint8_t msg_type);
const char *sentinel_topic_name(uint16_t topic_id);
void print_parsed_packet(const parsed_packet_t *pkt);

#endif /* SENTINEL_PARSER_H */
