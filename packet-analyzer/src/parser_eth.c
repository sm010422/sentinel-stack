/**
 * @file parser_eth.c
 * @brief Ethernet II 프레임 파서
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "parser.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <arpa/inet.h>

#define ETH_ALEN     6U
#define ETH_HDR_LEN  14U
#define ETHERTYPE_IP   0x0800U
#define ETHERTYPE_VLAN 0x8100U

int parse_ethernet(const uint8_t *data, size_t len, eth_frame_t *out)
{
    size_t offset;

    if (data == NULL || out == NULL || len < ETH_HDR_LEN) {
        return -1;
    }

    (void)memset(out, 0, sizeof(eth_frame_t));

    (void)memcpy(out->dst_mac, data + 0U, ETH_ALEN);
    (void)memcpy(out->src_mac, data + ETH_ALEN, ETH_ALEN);

    offset = ETH_HDR_LEN - 2U; /* EtherType 위치 (12번째 바이트) */
    out->ether_type = (uint16_t)((data[12] << 8) | data[13]);

    /* VLAN 태그 처리 (802.1Q) */
    if (out->ether_type == (uint16_t)ETHERTYPE_VLAN) {
        if (len < ETH_HDR_LEN + 4U) {
            return -1;
        }
        out->vlan_id    = (uint16_t)(((data[14] & 0x0FU) << 8) | data[15]);
        out->ether_type = (uint16_t)((data[16] << 8) | data[17]);
    }

    (void)offset;
    return 0;
}

void mac_to_str(const uint8_t *mac, char *buf, size_t buf_len)
{
    if (mac == NULL || buf == NULL || buf_len < 18U) {
        return;
    }

    (void)snprintf(buf, buf_len,
                   "%02X:%02X:%02X:%02X:%02X:%02X",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
