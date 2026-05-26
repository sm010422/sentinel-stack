/**
 * @file test_parser.c
 * @brief 패킷 파서 단위 테스트
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "../include/parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name, cond) \
    do { \
        printf("  [TEST] %-50s ... ", (name)); \
        if (cond) { printf("PASS\n"); g_pass++; } \
        else       { printf("FAIL\n"); g_fail++; } \
    } while (0)

/* ── 테스트용 원시 바이트 배열 ─────────────────────────────────── */

/* 최소 Ethernet + IPv4 + TCP 헤더 (payload 없음) */
static const uint8_t RAW_ETH_IP_TCP[] = {
    /* Ethernet: dst=FF:FF:FF:FF:FF:FF src=00:11:22:33:44:55 type=0x0800 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x00,0x11,0x22,0x33,0x44,0x55,
    0x08,0x00,
    /* IPv4: ver=4 ihl=5 ttl=64 proto=6(TCP) src=127.0.0.1 dst=127.0.0.1 */
    0x45,0x00, 0x00,0x28, /* total_len=40 */
    0x00,0x01, 0x40,0x00, /* id, flags */
    0x40,0x06, 0x00,0x00, /* ttl, proto=TCP, checksum */
    0x7F,0x00,0x00,0x01,  /* src=127.0.0.1 */
    0x7F,0x00,0x00,0x01,  /* dst=127.0.0.1 */
    /* TCP: src=52341 dst=9000 flags=SYN */
    0xCC,0x75, 0x23,0x28, /* src=52341, dst=9000 */
    0x00,0x00,0x00,0x01,  /* seq_num */
    0x00,0x00,0x00,0x00,  /* ack_num */
    0x50,0x02, 0xFF,0xFF, /* data_offset=20, SYN flag, window */
    0x00,0x00, 0x00,0x00  /* checksum, urgent */
};

/* sentinel 매직이 포함된 페이로드 (헤더만) */
static const uint8_t SENTINEL_HDR[] = {
    0x53,0x45,0x4E,0x54, /* magic = "SENT" */
    0x01,                /* version */
    0x01,                /* msg_type = DATA */
    0x00,0x00,0x00,0x01, /* seq_num = 1 */
    0x00,0x00,0x01,0x94,0x89,0x32,0x70,0x80, /* timestamp_ms */
    0x01,                /* src_node_id */
    0x02,                /* dst_node_id */
    0x00,0x01,           /* topic_id = SENSOR_DATA */
    0x00,0x00,0x00,0x40, /* payload_len = 64 */
    /* AES IV (12 bytes) */
    0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,
    0x11,0x22,0x33,0x44,0x55,0x66
};

/* ── 테스트 함수 ─────────────────────────────────────────────────── */

static void test_ethernet_parse(void)
{
    eth_frame_t eth;
    int ret;

    printf("\n[SUITE] Ethernet 파서\n");

    ret = parse_ethernet(RAW_ETH_IP_TCP, sizeof(RAW_ETH_IP_TCP), &eth);
    TEST("parse_ethernet 반환값 == 0", ret == 0);
    TEST("dst_mac[0] == 0xFF (브로드캐스트)", eth.dst_mac[0] == 0xFF);
    TEST("src_mac[0] == 0x00", eth.src_mac[0] == 0x00);
    TEST("ether_type == 0x0800 (IPv4)", eth.ether_type == 0x0800U);

    TEST("NULL 데이터 → 실패", parse_ethernet(NULL, 14, &eth) == -1);
    TEST("짧은 데이터 → 실패", parse_ethernet(RAW_ETH_IP_TCP, 5, &eth) == -1);
}

static void test_ip_parse(void)
{
    ip_header_t ip;
    size_t      ihl;
    int         ret;

    printf("\n[SUITE] IPv4 파서\n");

    /* Ethernet 헤더(14바이트) 이후부터 시작 */
    ret = parse_ip(RAW_ETH_IP_TCP + 14U,
                   sizeof(RAW_ETH_IP_TCP) - 14U, &ip, &ihl);

    TEST("parse_ip 반환값 == 0", ret == 0);
    TEST("version == 4", ip.version == 4U);
    TEST("protocol == 6 (TCP)", ip.protocol == 6U);
    TEST("ihl == 20", ihl == 20U);
    TEST("src_ip == 127.0.0.1", ip.src_ip == 0x7F000001U);
    TEST("dst_ip == 127.0.0.1", ip.dst_ip == 0x7F000001U);
}

static void test_tcp_parse(void)
{
    tcp_header_t tcp;
    size_t       hdr_len;
    int          ret;

    printf("\n[SUITE] TCP 파서\n");

    /* Ethernet(14) + IP(20) = 34바이트 이후 TCP */
    ret = parse_tcp(RAW_ETH_IP_TCP + 34U,
                    sizeof(RAW_ETH_IP_TCP) - 34U, &tcp, &hdr_len);

    TEST("parse_tcp 반환값 == 0", ret == 0);
    TEST("src_port == 52341", tcp.src_port == 52341U);
    TEST("dst_port == 9000", tcp.dst_port == 9000U);
    TEST("SYN 플래그 설정", (tcp.flags & TCP_FLAG_SYN) != 0U);
    TEST("ACK 플래그 없음", (tcp.flags & TCP_FLAG_ACK) == 0U);
    TEST("hdr_len == 20", hdr_len == 20U);
}

static void test_sentinel_parse(void)
{
    parsed_sentinel_t s;
    int               ret;

    printf("\n[SUITE] sentinel 프로토콜 파서\n");

    ret = parse_sentinel(SENTINEL_HDR, sizeof(SENTINEL_HDR), &s);

    TEST("parse_sentinel 반환값 == 0", ret == 0);
    TEST("magic_valid == true", s.magic_valid);
    TEST("version == 1", s.version == 1U);
    TEST("msg_type == DATA (0x01)", s.msg_type == 0x01U);
    TEST("src_node_id == 1", s.src_node_id == 1U);
    TEST("dst_node_id == 2", s.dst_node_id == 2U);
    TEST("topic_id == SENSOR_DATA (0x0001)", s.topic_id == 0x0001U);
    TEST("payload_len == 64", s.payload_len == 64U);

    /* 잘못된 매직 */
    {
        uint8_t bad[sizeof(SENTINEL_HDR)];
        (void)memcpy(bad, SENTINEL_HDR, sizeof(SENTINEL_HDR));
        bad[0] = 0xDEU; /* 매직 파괴 */
        TEST("잘못된 매직 → 실패", parse_sentinel(bad, sizeof(bad), &s) == -1);
    }
}

static void test_full_packet_parse(void)
{
    parsed_packet_t pkt;
    int             ret;

    printf("\n[SUITE] 통합 패킷 파서\n");

    ret = parse_packet(RAW_ETH_IP_TCP, sizeof(RAW_ETH_IP_TCP), &pkt);

    TEST("parse_packet 반환값 == 0", ret == 0);
    TEST("has_eth == true", pkt.has_eth);
    TEST("has_ip  == true", pkt.has_ip);
    TEST("has_tcp == true", pkt.has_tcp);
    TEST("has_udp == false", !pkt.has_udp);
    TEST("has_sentinel == false (페이로드 없음)", !pkt.has_sentinel);
    TEST("raw_len == sizeof(RAW_ETH_IP_TCP)",
         pkt.raw_len == (uint32_t)sizeof(RAW_ETH_IP_TCP));
}

static void test_utilities(void)
{
    char buf[32];

    printf("\n[SUITE] 유틸리티 함수\n");

    ip_to_str(0x7F000001U, buf, sizeof(buf));
    TEST("ip_to_str(127.0.0.1)", strcmp(buf, "127.0.0.1") == 0);

    ip_to_str(0xC0A80001U, buf, sizeof(buf));
    TEST("ip_to_str(192.168.0.1)", strcmp(buf, "192.168.0.1") == 0);

    {
        uint8_t mac[6] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
        mac_to_str(mac, buf, sizeof(buf));
        TEST("mac_to_str(AA:BB:CC:DD:EE:FF)",
             strcmp(buf, "AA:BB:CC:DD:EE:FF") == 0);
    }

    TEST("sentinel_msg_type_name(0x01) == DATA",
         strcmp(sentinel_msg_type_name(0x01U), "DATA") == 0);
    TEST("sentinel_topic_name(0x0002) == STATUS",
         strcmp(sentinel_topic_name(0x0002U), "STATUS") == 0);
}

/* ── 진입점 ────────────────────────────────────────────────────── */

int main(void)
{
    printf("══════════════════════════════════════════════════\n");
    printf("  sentinel-stack | 패킷 파서 단위 테스트\n");
    printf("══════════════════════════════════════════════════\n");

    test_ethernet_parse();
    test_ip_parse();
    test_tcp_parse();
    test_sentinel_parse();
    test_full_packet_parse();
    test_utilities();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  결과: %d 통과 / %d 실패\n", g_pass, g_fail);
    printf("  상태: %s\n", (g_fail == 0) ? "ALL PASS ✓" : "FAIL ✗");
    printf("══════════════════════════════════════════════════\n");

    return (g_fail == 0) ? 0 : 1;
}
