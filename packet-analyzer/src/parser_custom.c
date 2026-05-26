/**
 * @file parser_custom.c
 * @brief sentinel 커스텀 프로토콜 파서 및 통합 패킷 파서
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "parser.h"

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#define SENTINEL_MAGIC      0x53454E54U
#define SENTINEL_PORT_TCP   9000U
#define SENTINEL_PORT_UDP   9001U
#define SENTINEL_HDR_LEN    38U

/* ETH 헤더 크기 */
#define ETH_HDR_LEN    14U
#define ETH_VLAN_LEN   18U
#define ETHERTYPE_IP   0x0800U
#define ETHERTYPE_VLAN 0x8100U

/* =========================================================================
 * CRC-32 (protocol.c 와 동일한 구현 — 별도 선언으로 독립성 유지)
 * ========================================================================= */

static uint32_t crc32_compute(const uint8_t *data, size_t len)
{
    static uint32_t table[256];
    static int      ready = 0;
    uint32_t        crc   = 0xFFFFFFFFU;
    size_t          i;
    uint32_t        j;

    if (ready == 0) {
        for (i = 0U; i < 256U; i++) {
            uint32_t v = (uint32_t)i;
            for (j = 0U; j < 8U; j++) {
                v = ((v & 1U) != 0U) ? (v >> 1) ^ 0xEDB88320U : (v >> 1);
            }
            table[i] = v;
        }
        ready = 1;
    }

    for (i = 0U; i < len; i++) {
        crc = (crc >> 8) ^ table[(crc ^ (uint32_t)data[i]) & 0xFFU];
    }

    return crc ^ 0xFFFFFFFFU;
}

/* =========================================================================
 * sentinel 프로토콜 식별 및 파싱
 * ========================================================================= */

const char *sentinel_msg_type_name(uint8_t msg_type)
{
    switch (msg_type) {
        case 0x01U: return "DATA";
        case 0x02U: return "STATUS";
        case 0x03U: return "CMD";
        case 0x04U: return "HEARTBEAT";
        case 0x05U: return "ACK";
        default:    return "UNKNOWN";
    }
}

const char *sentinel_topic_name(uint16_t topic_id)
{
    switch (topic_id) {
        case 0x0001U: return "SENSOR_DATA";
        case 0x0002U: return "STATUS";
        case 0x0003U: return "CMD";
        case 0x0004U: return "HEARTBEAT";
        default:      return "UNKNOWN_TOPIC";
    }
}

int parse_sentinel(const uint8_t *data, size_t len, parsed_sentinel_t *out)
{
    uint32_t magic;
    uint32_t payload_len;
    uint32_t rx_crc;
    uint32_t calc_crc;
    size_t   expected_total;

    if (data == NULL || out == NULL || len < SENTINEL_HDR_LEN) {
        return -1;
    }

    (void)memset(out, 0, sizeof(parsed_sentinel_t));

    /* 매직 번호 */
    magic = ((uint32_t)data[0] << 24)
          | ((uint32_t)data[1] << 16)
          | ((uint32_t)data[2] <<  8)
          |  (uint32_t)data[3];

    out->magic       = magic;
    out->magic_valid = (magic == SENTINEL_MAGIC);

    if (!out->magic_valid) {
        return -1;
    }

    out->version     = data[4];
    out->msg_type    = data[5];

    out->seq_num = ((uint32_t)data[6] << 24)
                 | ((uint32_t)data[7] << 16)
                 | ((uint32_t)data[8] <<  8)
                 |  (uint32_t)data[9];

    out->timestamp_ms = ((uint64_t)data[10] << 56)
                      | ((uint64_t)data[11] << 48)
                      | ((uint64_t)data[12] << 40)
                      | ((uint64_t)data[13] << 32)
                      | ((uint64_t)data[14] << 24)
                      | ((uint64_t)data[15] << 16)
                      | ((uint64_t)data[16] <<  8)
                      |  (uint64_t)data[17];

    out->src_node_id = data[18];
    out->dst_node_id = data[19];

    out->topic_id = (uint16_t)((data[20] << 8) | data[21]);

    payload_len = ((uint32_t)data[22] << 24)
                | ((uint32_t)data[23] << 16)
                | ((uint32_t)data[24] <<  8)
                |  (uint32_t)data[25];

    out->payload_len = payload_len;

    /* CRC 검증 (패킷이 충분히 길 때만) */
    expected_total = SENTINEL_HDR_LEN + (size_t)payload_len + 16U /* tag */ + 4U /* crc */;

    if (len >= expected_total) {
        rx_crc   = ((uint32_t)data[expected_total - 4U] << 24)
                 | ((uint32_t)data[expected_total - 3U] << 16)
                 | ((uint32_t)data[expected_total - 2U] <<  8)
                 |  (uint32_t)data[expected_total - 1U];
        calc_crc = crc32_compute(data, expected_total - 4U);
        out->crc_valid = (rx_crc == calc_crc);
    } else {
        out->crc_valid = false;
    }

    return 0;
}

/* =========================================================================
 * 통합 패킷 파서
 * ========================================================================= */

int parse_packet(const uint8_t *data, size_t len, parsed_packet_t *out)
{
    size_t eth_len;
    size_t ip_hdr_len;
    size_t tcp_hdr_len;
    const uint8_t *ip_data;
    const uint8_t *transport_data;
    size_t         transport_len;

    if (data == NULL || out == NULL || len == 0U) {
        return -1;
    }

    (void)memset(out, 0, sizeof(parsed_packet_t));
    out->raw_len = (uint32_t)len;

    /* ── Ethernet ─────────────────────────────────────────── */
    if (parse_ethernet(data, len, &out->eth) != 0) {
        return -1;
    }
    out->has_eth = true;

    /* VLAN 여부에 따라 Ethernet 헤더 크기 결정 */
    eth_len = (out->eth.vlan_id != 0U) ? ETH_VLAN_LEN : ETH_HDR_LEN;

    if (out->eth.ether_type != (uint16_t)ETHERTYPE_IP) {
        return 0; /* IPv4만 파싱 */
    }

    if (len <= eth_len) {
        return -1;
    }

    /* ── IPv4 ─────────────────────────────────────────────── */
    ip_data = data + eth_len;
    if (parse_ip(ip_data, len - eth_len, &out->ip, &ip_hdr_len) != 0) {
        return 0;
    }
    out->has_ip = true;

    transport_data = ip_data + ip_hdr_len;
    transport_len  = len - eth_len - ip_hdr_len;

    /* ── TCP ──────────────────────────────────────────────── */
    if (out->ip.protocol == 6U) { /* TCP */
        if (parse_tcp(transport_data, transport_len,
                      &out->tcp, &tcp_hdr_len) == 0) {
            out->has_tcp = true;

            /* 애플리케이션 페이로드 */
            out->payload_offset = (uint32_t)(eth_len + ip_hdr_len + tcp_hdr_len);
            out->payload_len    = (uint32_t)(transport_len > tcp_hdr_len
                                             ? transport_len - tcp_hdr_len : 0U);

            /* sentinel 프로토콜 탐지 (포트 9000) */
            if ((out->tcp.src_port == (uint16_t)SENTINEL_PORT_TCP ||
                 out->tcp.dst_port == (uint16_t)SENTINEL_PORT_TCP) &&
                (out->payload_len >= SENTINEL_HDR_LEN)) {
                const uint8_t *payload = data + out->payload_offset;
                if (parse_sentinel(payload, out->payload_len,
                                   &out->sentinel) == 0) {
                    out->has_sentinel = true;
                }
            }
        }
    }
    /* ── UDP ──────────────────────────────────────────────── */
    else if (out->ip.protocol == 17U) { /* UDP */
        if (parse_udp(transport_data, transport_len, &out->udp) == 0) {
            out->has_udp = true;

            out->payload_offset = (uint32_t)(eth_len + ip_hdr_len + 8U);
            out->payload_len    = (uint32_t)(transport_len > 8U
                                             ? transport_len - 8U : 0U);

            /* sentinel 프로토콜 탐지 (포트 9001) */
            if ((out->udp.src_port == (uint16_t)SENTINEL_PORT_UDP ||
                 out->udp.dst_port == (uint16_t)SENTINEL_PORT_UDP) &&
                (out->payload_len >= SENTINEL_HDR_LEN)) {
                const uint8_t *payload = data + out->payload_offset;
                if (parse_sentinel(payload, out->payload_len,
                                   &out->sentinel) == 0) {
                    out->has_sentinel = true;
                }
            }
        }
    }

    return 0;
}

/* =========================================================================
 * 출력 함수
 * ========================================================================= */

void print_parsed_packet(const parsed_packet_t *pkt)
{
    char src_ip[16];
    char dst_ip[16];

    if (pkt == NULL || !pkt->has_ip) {
        return;
    }

    ip_to_str(pkt->ip.src_ip, src_ip, sizeof(src_ip));
    ip_to_str(pkt->ip.dst_ip, dst_ip, sizeof(dst_ip));

    if (pkt->has_tcp) {
        printf("[%ld.%06ld] ETH | IP %s → %s | TCP %u → %u",
               (long)pkt->timestamp.tv_sec,
               (long)pkt->timestamp.tv_usec,
               src_ip, dst_ip,
               pkt->tcp.src_port, pkt->tcp.dst_port);

        if (pkt->has_sentinel) {
            const parsed_sentinel_t *s = &pkt->sentinel;
            printf("\n             └─ SENTINEL v%u | NODE:%u→%u | TOPIC:%s"
                   " | len=%u | CRC:%s\n",
                   s->version,
                   s->src_node_id, s->dst_node_id,
                   sentinel_topic_name(s->topic_id),
                   s->payload_len,
                   s->crc_valid ? "OK" : "FAIL");
        } else {
            printf(" | len=%u\n", pkt->raw_len);
        }
    } else if (pkt->has_udp) {
        printf("[%ld.%06ld] ETH | IP %s → %s | UDP %u → %u",
               (long)pkt->timestamp.tv_sec,
               (long)pkt->timestamp.tv_usec,
               src_ip, dst_ip,
               pkt->udp.src_port, pkt->udp.dst_port);

        if (pkt->has_sentinel) {
            const parsed_sentinel_t *s = &pkt->sentinel;
            printf("\n             └─ SENTINEL v%u | NODE:%u→%u | TOPIC:%s"
                   " | len=%u | %s\n",
                   s->version,
                   s->src_node_id, s->dst_node_id,
                   sentinel_topic_name(s->topic_id),
                   s->payload_len,
                   (s->dst_node_id == 0xFFU) ? "MULTICAST" : "UNICAST");
        } else {
            printf(" | len=%u\n", pkt->raw_len);
        }
    }
}
