/**
 * @file tcp_transport.c
 * @brief TCP 신뢰 채널 전송 레이어
 *
 * MIL-STD-1553 이중화 버스의 신뢰 채널(Primary Bus)에 해당합니다.
 * 명령(CMD)과 상태(STATUS) 메시지를 TCP로 전송하여 신뢰성을 보장합니다.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "protocol.h"
#include "tcp_transport.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>  /* TCP_NODELAY */
#include <arpa/inet.h>

/* =========================================================================
 * 내부 헬퍼
 * ========================================================================= */

/**
 * @brief TCP_NODELAY 옵션 설정 (Nagle 알고리즘 비활성화)
 *
 * 낮은 지연이 중요한 전술 통신에서는 Nagle 알고리즘을 비활성화합니다.
 */
static int set_tcp_nodelay(int fd)
{
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

/**
 * @brief SO_REUSEADDR 설정 (빠른 재시작 지원)
 */
static int set_reuse_addr(int fd)
{
    int opt = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

/* =========================================================================
 * 공개 함수
 * ========================================================================= */

/**
 * @brief TCP 서버 소켓 생성 및 바인딩
 *
 * @param port 바인딩할 포트
 * @return 서버 소켓 fd, -1 실패
 */
int tcp_create_server(uint32_t port)
{
    struct sockaddr_in addr;
    int                fd;
    int                backlog = 8;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("[TCP] socket 생성 실패");
        return -1;
    }

    if (set_reuse_addr(fd) != 0) {
        perror("[TCP] SO_REUSEADDR 설정 실패");
        (void)close(fd);
        return -1;
    }

    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("[TCP] bind 실패");
        (void)close(fd);
        return -1;
    }

    if (listen(fd, backlog) != 0) {
        perror("[TCP] listen 실패");
        (void)close(fd);
        return -1;
    }

    printf("[TCP] 서버 소켓 열림: 포트 %u (fd=%d)\n", port, fd);
    return fd;
}

/**
 * @brief 새 TCP 클라이언트 연결 수락
 *
 * @param server_fd 서버 소켓 fd
 * @param remote_addr [출력] 클라이언트 주소 문자열 (NULL 무시)
 * @return 클라이언트 소켓 fd, -1 실패
 */
int tcp_accept(int server_fd, char *remote_addr)
{
    struct sockaddr_in client;
    socklen_t          len;
    int                client_fd;

    len       = sizeof(client);
    client_fd = accept(server_fd, (struct sockaddr *)&client, &len);

    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("[TCP] accept 실패");
        }
        return -1;
    }

    (void)set_tcp_nodelay(client_fd);

    if (remote_addr != NULL) {
        (void)inet_ntop(AF_INET, &client.sin_addr, remote_addr, 64);
    }

    printf("[TCP] 클라이언트 연결: %s (fd=%d)\n",
           remote_addr ? remote_addr : "unknown", client_fd);

    return client_fd;
}

/**
 * @brief 피어 TCP 서버에 연결
 *
 * @param addr 피어 주소 (IPv4 문자열)
 * @param port 피어 포트
 * @return 연결된 소켓 fd, -1 실패
 */
int tcp_connect(const char *addr, uint32_t port)
{
    struct sockaddr_in server;
    int                fd;

    assert(addr != NULL);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("[TCP] socket 생성 실패");
        return -1;
    }

    (void)memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, addr, &server.sin_addr) <= 0) {
        fprintf(stderr, "[TCP] 잘못된 주소: %s\n", addr);
        (void)close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&server, sizeof(server)) != 0) {
        fprintf(stderr, "[TCP] 연결 실패: %s:%u — %s\n",
                addr, port, strerror(errno));
        (void)close(fd);
        return -1;
    }

    (void)set_tcp_nodelay(fd);

    printf("[TCP] 연결 성공: %s:%u (fd=%d)\n", addr, port, fd);
    return fd;
}

/**
 * @brief sentinel 패킷을 TCP로 전송
 *
 * 부분 전송(partial send) 처리를 위해 루프로 재시도합니다.
 *
 * @param fd  전송할 소켓 fd
 * @param pkt 전송할 패킷
 * @return 0 성공, -1 실패
 */
int tcp_send_packet(int fd, const sentinel_packet_t *pkt)
{
    static uint8_t tx_buf[SENTINEL_MAX_PACKET_SIZE];
    size_t         total_len;
    size_t         sent;
    ssize_t        n;

    if (fd < 0 || pkt == NULL) {
        return -1;
    }

    if (sentinel_packet_serialize(pkt, tx_buf, sizeof(tx_buf), &total_len) != 0) {
        fprintf(stderr, "[TCP] 직렬화 실패\n");
        return -1;
    }

    sent = 0U;
    while (sent < total_len) {
        n = send(fd, tx_buf + sent, total_len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue; /* 시그널 인터럽트: 재시도 */
            }
            fprintf(stderr, "[TCP] send 실패: %s\n", strerror(errno));
            return -1;
        }
        if (n == 0) {
            return -1; /* 연결 종료 */
        }
        sent += (size_t)n;
    }

    return 0;
}

/**
 * @brief TCP 소켓에서 sentinel 패킷 수신
 *
 * 헤더 크기를 먼저 읽고 payload_len 을 파악한 후 나머지를 수신합니다.
 * 부분 수신(partial recv) 처리를 위해 루프로 재시도합니다.
 *
 * @param fd  수신할 소켓 fd
 * @param pkt [출력] 수신된 패킷
 * @return 0 성공, -1 실패 또는 연결 종료
 */
int tcp_recv_packet(int fd, sentinel_packet_t *pkt)
{
    static uint8_t rx_buf[SENTINEL_MAX_PACKET_SIZE];
    uint32_t       payload_len;
    size_t         total_expected;
    size_t         received;
    ssize_t        n;

    if (fd < 0 || pkt == NULL) {
        return -1;
    }

    /* 1단계: 헤더 수신 */
    received = 0U;
    while (received < SENTINEL_HEADER_SIZE) {
        n = recv(fd, rx_buf + received, SENTINEL_HEADER_SIZE - received, 0);
        if (n <= 0) {
            if (n == 0) {
                printf("[TCP] 연결 종료 (fd=%d)\n", fd);
            } else if (errno != EINTR) {
                fprintf(stderr, "[TCP] recv 헤더 실패: %s\n", strerror(errno));
            }
            return -1;
        }
        received += (size_t)n;
    }

    /* 매직 빠른 검증 */
    uint32_t magic = ((uint32_t)rx_buf[0] << 24)
                   | ((uint32_t)rx_buf[1] << 16)
                   | ((uint32_t)rx_buf[2] <<  8)
                   |  (uint32_t)rx_buf[3];
    if (magic != SENTINEL_MAGIC) {
        fprintf(stderr, "[TCP] 잘못된 매직: 0x%08X\n", magic);
        return -1;
    }

    /* payload_len 추출 (헤더 오프셋 30) */
    {
        uint32_t plen;
        (void)memcpy(&plen, rx_buf + 30U, 4U);
        payload_len = (uint32_t)__builtin_bswap32(plen); /* network to host */
    }

    if (payload_len > SENTINEL_MAX_PAYLOAD) {
        fprintf(stderr, "[TCP] payload_len 초과: %u\n", payload_len);
        return -1;
    }

    /* 2단계: 나머지 (payload + tag + crc) 수신 */
    total_expected = SENTINEL_HEADER_SIZE + (size_t)payload_len
                     + SENTINEL_AES_TAG_LEN + 4U;

    while (received < total_expected) {
        n = recv(fd, rx_buf + received, total_expected - received, 0);
        if (n == 0) {
            printf("[TCP] 연결 종료 (fd=%d)\n", fd);
            return -1;
        }
        if (n < 0) {
            if (errno != EINTR) {
                fprintf(stderr, "[TCP] recv 페이로드 실패: %s\n", strerror(errno));
            }
            return -1;
        }
        received += (size_t)n;
    }

    return sentinel_packet_deserialize(rx_buf, received, pkt);
}

/**
 * @brief TCP 소켓 닫기
 *
 * @param fd 닫을 소켓 fd
 */
void tcp_close(int fd)
{
    if (fd >= 0) {
        (void)shutdown(fd, SHUT_RDWR);
        (void)close(fd);
    }
}
