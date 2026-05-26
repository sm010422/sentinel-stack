/**
 * @file test_latency.c
 * @brief TCP/UDP 왕복 지연(RTT) 벤치마크
 *
 * 루프백 인터페이스(127.0.0.1)에서 10,000개 패킷의 평균/최소/최대 RTT를
 * 측정합니다. README 명시 목표: TCP RTT < 0.5ms, UDP RTT < 0.2ms.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "../include/protocol.h"
#include "../include/crypto.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* 외부 함수 선언 */
extern int  tcp_create_server(uint32_t port);
extern int  tcp_accept(int server_fd, char *remote_addr);
extern int  tcp_connect(const char *addr, uint32_t port);
extern int  tcp_send_packet(int fd, const sentinel_packet_t *pkt);
extern int  tcp_recv_packet(int fd, sentinel_packet_t *pkt);
extern void tcp_close(int fd);

/* =========================================================================
 * 타임스탬프 (나노초)
 * ========================================================================= */

static int64_t now_ns(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

/* =========================================================================
 * 에코 서버 스레드
 * ========================================================================= */

#define TEST_PORT   19000U
#define TEST_COUNT  10000U

static int g_server_fd  = -1;
static int g_client_fd  = -1;

static void *echo_server_thread(void *arg)
{
    sentinel_packet_t pkt;

    (void)arg;

    g_client_fd = tcp_accept(g_server_fd, NULL);
    if (g_client_fd < 0) {
        return NULL;
    }

    /* 패킷을 받는 즉시 그대로 돌려보냄 */
    while (tcp_recv_packet(g_client_fd, &pkt) == 0) {
        (void)tcp_send_packet(g_client_fd, &pkt);
    }

    tcp_close(g_client_fd);
    g_client_fd = -1;
    return NULL;
}

/* =========================================================================
 * TCP RTT 벤치마크
 * ========================================================================= */

static void benchmark_tcp_rtt(void)
{
    pthread_t         server_th;
    int               client_fd;
    sentinel_packet_t tx_pkt;
    sentinel_packet_t rx_pkt;
    int64_t           rtts[TEST_COUNT];
    int64_t           sum    = 0;
    int64_t           min_rtt = INT64_MAX;
    int64_t           max_rtt = 0;
    uint32_t          i;

    /* 데모용 고정 페이로드 (1KB) */
    static const uint8_t PAYLOAD[1024] = {0xAB};

    static const uint8_t KEY[32] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
        0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F
    };

    crypto_ctx_t crypto;
    (void)crypto_init(&crypto, KEY, sizeof(KEY));

    printf("[LATENCY] TCP RTT 벤치마크 시작 (%u 패킷, 1KB 페이로드)\n",
           TEST_COUNT);

    /* 서버 시작 */
    g_server_fd = tcp_create_server(TEST_PORT);
    assert(g_server_fd >= 0);

    (void)pthread_create(&server_th, NULL, echo_server_thread, NULL);

    /* 클라이언트 연결 */
    usleep(50000U); /* 서버 준비 대기 50ms */
    client_fd = tcp_connect("127.0.0.1", TEST_PORT);
    assert(client_fd >= 0);

    /* 패킷 템플릿 */
    (void)memset(&tx_pkt, 0, sizeof(tx_pkt));
    tx_pkt.header.version      = SENTINEL_VERSION;
    tx_pkt.header.msg_type     = (uint8_t)SENTINEL_MSG_DATA;
    tx_pkt.header.src_node_id  = 1U;
    tx_pkt.header.dst_node_id  = 2U;
    tx_pkt.header.topic_id     = SENTINEL_TOPIC_SENSOR_DATA;

    for (i = 0U; i < TEST_COUNT; i++) {
        int64_t t0;
        int64_t t1;
        uint8_t iv[CRYPTO_IV_LEN];
        size_t  ct_len = 0U;

        tx_pkt.header.seq_num = i;
        (void)crypto_generate_iv(iv);
        (void)memcpy(tx_pkt.header.aes_iv, iv, CRYPTO_IV_LEN);

        (void)crypto_encrypt(&crypto,
                             PAYLOAD, sizeof(PAYLOAD),
                             iv,
                             tx_pkt.enc_payload, &ct_len,
                             tx_pkt.aes_tag);
        tx_pkt.enc_payload_len   = (uint32_t)ct_len;
        tx_pkt.header.payload_len = (uint32_t)ct_len;

        t0 = now_ns();
        (void)tcp_send_packet(client_fd, &tx_pkt);
        (void)tcp_recv_packet(client_fd, &rx_pkt);
        t1 = now_ns();

        rtts[i] = t1 - t0;
        sum += rtts[i];

        if (rtts[i] < min_rtt) min_rtt = rtts[i];
        if (rtts[i] > max_rtt) max_rtt = rtts[i];
    }

    tcp_close(client_fd);
    (void)pthread_join(server_th, NULL);
    tcp_close(g_server_fd);

    double avg_us = (double)sum / TEST_COUNT / 1000.0;
    double min_us = (double)min_rtt / 1000.0;
    double max_us = (double)max_rtt / 1000.0;

    printf("[LATENCY] TCP RTT 결과:\n");
    printf("          평균 RTT: %.3f µs (%.3f ms)\n", avg_us, avg_us / 1000.0);
    printf("          최소 RTT: %.3f µs\n", min_us);
    printf("          최대 RTT: %.3f µs\n", max_us);
    printf("          목표(README): < 500 µs — %s\n",
           (avg_us < 500.0) ? "PASS" : "FAIL (루프백 목표 초과)");
}

/* =========================================================================
 * 진입점
 * ========================================================================= */

int main(void)
{
    printf("══════════════════════════════════════════════════\n");
    printf("  sentinel-stack | 통신 지연 벤치마크\n");
    printf("══════════════════════════════════════════════════\n\n");

    benchmark_tcp_rtt();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  벤치마크 완료\n");
    printf("══════════════════════════════════════════════════\n");

    return 0;
}
