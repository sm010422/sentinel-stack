/**
 * @file parser_ip.c
 * @brief IPv4 헤더 파서 및 UDP 파서
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "parser.h"

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#define IP_MIN_HDR_LEN 20U
#define IP_VERSION_4   4U

int parse_ip(const uint8_t *data, size_t len, ip_header_t *out, size_t *ip_hdr_len)
{
    uint8_t  ihl;

    if (data == NULL || out == NULL || len < IP_MIN_HDR_LEN) {
        return -1;
    }

    (void)memset(out, 0, sizeof(ip_header_t));

    out->version = (data[0] >> 4) & 0x0FU;
    ihl          = (data[0] & 0x0FU) * 4U;

    if (out->version != IP_VERSION_4 || ihl < IP_MIN_HDR_LEN || (size_t)ihl > len) {
        return -1;
    }

    out->ihl       = ihl;
    out->ttl       = data[8];
    out->protocol  = data[9];
    out->total_len = (uint16_t)((data[2] << 8) | data[3]);
    out->id        = (uint16_t)((data[4] << 8) | data[5]);
    out->flags     = (data[6] >> 5) & 0x07U;

    (void)memcpy(&out->src_ip, data + 12U, 4U);
    (void)memcpy(&out->dst_ip, data + 16U, 4U);
    out->src_ip = ntohl(out->src_ip);
    out->dst_ip = ntohl(out->dst_ip);

    if (ip_hdr_len != NULL) {
        *ip_hdr_len = (size_t)ihl;
    }

    return 0;
}

int parse_udp(const uint8_t *data, size_t len, udp_header_t *out)
{
    if (data == NULL || out == NULL || len < 8U) {
        return -1;
    }

    out->src_port = (uint16_t)((data[0] << 8) | data[1]);
    out->dst_port = (uint16_t)((data[2] << 8) | data[3]);
    out->length   = (uint16_t)((data[4] << 8) | data[5]);

    return 0;
}

void ip_to_str(uint32_t ip, char *buf, size_t buf_len)
{
    if (buf == NULL || buf_len < 16U) {
        return;
    }

    (void)snprintf(buf, buf_len, "%u.%u.%u.%u",
                   (ip >> 24) & 0xFFU,
                   (ip >> 16) & 0xFFU,
                   (ip >>  8) & 0xFFU,
                    ip        & 0xFFU);
}
