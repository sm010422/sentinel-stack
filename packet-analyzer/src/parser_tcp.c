/**
 * @file parser_tcp.c
 * @brief TCP 헤더 파서
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "parser.h"

#include <string.h>

#define TCP_MIN_HDR_LEN 20U

int parse_tcp(const uint8_t *data, size_t len, tcp_header_t *out, size_t *tcp_hdr_len)
{
    uint8_t data_offset;

    if (data == NULL || out == NULL || len < TCP_MIN_HDR_LEN) {
        return -1;
    }

    (void)memset(out, 0, sizeof(tcp_header_t));

    out->src_port = (uint16_t)((data[0] << 8) | data[1]);
    out->dst_port = (uint16_t)((data[2] << 8) | data[3]);

    out->seq_num = ((uint32_t)data[4] << 24)
                 | ((uint32_t)data[5] << 16)
                 | ((uint32_t)data[6] <<  8)
                 |  (uint32_t)data[7];

    out->ack_num = ((uint32_t)data[8]  << 24)
                 | ((uint32_t)data[9]  << 16)
                 | ((uint32_t)data[10] <<  8)
                 |  (uint32_t)data[11];

    data_offset      = (data[12] >> 4) * 4U;
    out->data_offset = data_offset;
    out->flags       = data[13];
    out->window      = (uint16_t)((data[14] << 8) | data[15]);

    if ((size_t)data_offset < TCP_MIN_HDR_LEN || (size_t)data_offset > len) {
        return -1;
    }

    if (tcp_hdr_len != NULL) {
        *tcp_hdr_len = (size_t)data_offset;
    }

    return 0;
}
