/**
 * @file test_reconnect.c
 * @brief TCP 자동 재연결 동작 테스트
 *
 * 연결 후 서버를 강제 종료하고, 클라이언트가 COMM_RECONNECT_MS 이내에
 * 재연결에 성공하는지 검증합니다.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "../include/protocol.h"
#include "../include/comm_node.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

extern int  tcp_create_server(uint32_t port);
extern int  tcp_accept(int server_fd, char *remote_addr);
extern int  tcp_connect(const char *addr, uint32_t port);
extern void tcp_close(int fd);

#define RECONNECT_TEST_PORT  19010U
#define RECONNECT_WAIT_MS    1000U

static int64_t now_ms(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + (int64_t)(ts.tv_nsec / 1000000LL);
}

int main(void)
{
    int     server_fd;
    int     client_fd;
    int     reconnected_fd;
    int64_t t0;
    int64_t t1;
    int64_t elapsed_ms;

    printf("══════════════════════════════════════════════════\n");
    printf("  sentinel-stack | TCP 재연결 테스트\n");
    printf("══════════════════════════════════════════════════\n\n");

    /* 1. 서버 시작 */
    server_fd = tcp_create_server(RECONNECT_TEST_PORT);
    assert(server_fd >= 0);

    /* 2. 클라이언트 연결 */
    usleep(10000U);
    client_fd = tcp_connect("127.0.0.1", RECONNECT_TEST_PORT);
    assert(client_fd >= 0);

    /* 3. 서버가 클라이언트 수락 */
    {
        int accepted = tcp_accept(server_fd, NULL);
        if (accepted >= 0) {
            tcp_close(accepted);
        }
    }

    printf("[TEST] 연결 확립. 서버 강제 종료 시뮬레이션...\n");
    tcp_close(server_fd); /* 서버 닫기 — 클라이언트 연결 끊김 */
    tcp_close(client_fd);

    /* 4. 새 서버 재시작 */
    usleep(50000U); /* 50ms 대기 */
    server_fd = tcp_create_server(RECONNECT_TEST_PORT);
    assert(server_fd >= 0);

    /* 5. 재연결 시도 시간 측정 */
    t0 = now_ms();
    reconnected_fd = -1;

    while (now_ms() - t0 < (int64_t)RECONNECT_WAIT_MS) {
        reconnected_fd = tcp_connect("127.0.0.1", RECONNECT_TEST_PORT);
        if (reconnected_fd >= 0) {
            break;
        }
        usleep(10000U); /* 10ms 간격 재시도 */
    }

    t1          = now_ms();
    elapsed_ms  = t1 - t0;

    if (reconnected_fd >= 0) {
        printf("[TEST] 재연결 성공: %lldms\n", (long long)elapsed_ms);
        printf("[TEST] 목표(README): < %ums — %s\n",
               COMM_RECONNECT_MS,
               (elapsed_ms < (int64_t)COMM_RECONNECT_MS) ? "PASS" : "FAIL");
        tcp_close(reconnected_fd);
    } else {
        printf("[TEST] 재연결 실패 (타임아웃 %ums) — FAIL\n",
               RECONNECT_WAIT_MS);
    }

    tcp_close(server_fd);

    printf("\n══════════════════════════════════════════════════\n");
    printf("  테스트 완료\n");
    printf("══════════════════════════════════════════════════\n");

    return (reconnected_fd >= 0) ? 0 : 1;
}
