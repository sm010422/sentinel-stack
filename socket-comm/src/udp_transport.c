/**
 * @file udp_transport.c
 * @brief UDP 고속 채널 전송 레이어
 *
 * MIL-STD-1553 이중화 버스의 고속 채널(Secondary Bus)에 해당합니다.
 * 센서 데이터, Heartbeat 등 지연에 민감한 메시지를 UDP로 전송합니다.
 * 멀티캐스트 그룹을 통해 다수의 수신자에게 동시 전달합니다.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "protocol.h"
#include "udp_transport.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* =========================================================================
 * 공개 함수
 * ========================================================================= */

/**
 * @brief UDP 소켓 생성 및 포트 바인딩
 *
 * @param port 바인딩할 로컬 포트 (0이면 커널이 자동 할당)
 * @return UDP 소켓 fd, -1 실패
 */
int udp_create_socket(uint32_t port)
{
    struct sockaddr_in addr;
    int                fd;
    int                opt = 1;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("[UDP] socket 생성 실패");
        return -1;
    }

    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 멀티캐스트 그룹 참여를 위해 SO_REUSEPORT 설정 */
#ifdef SO_REUSEPORT
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    if (port > 0U) {
        (void)memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons((uint16_t)port);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            perror("[UDP] bind 실패");
            (void)close(fd);
            return -1;
        }
    }

    printf("[UDP] 소켓 생성: 포트 %u (fd=%d)\n", port, fd);
    return fd;
}

/**
 * @brief sentinel 패킷을 UDP로 전송
 *
 * UDP는 단일 데이터그램으로 전송합니다 (부분 전송 없음).
 * SENTINEL_MAX_PACKET_SIZE 를 초과하는 패킷은 단편화될 수 있습니다.
 *
 * @param fd    UDP 소켓 fd
 * @param addr  목적지 주소
 * @param port  목적지 포트
 * @param pkt   전송할 패킷
 * @return 0 성공, -1 실패
 */
int udp_send_packet(int fd, const char *addr, uint32_t port,
                    const sentinel_packet_t *pkt)
{
    static uint8_t     tx_buf[SENTINEL_MAX_PACKET_SIZE];
    struct sockaddr_in dst;
    size_t             total_len;
    ssize_t            n;

    assert(addr != NULL);
    assert(pkt  != NULL);

    if (fd < 0) {
        return -1;
    }

    if (sentinel_packet_serialize(pkt, tx_buf, sizeof(tx_buf), &total_len) != 0) {
        fprintf(stderr, "[UDP] 직렬화 실패\n");
        return -1;
    }

    (void)memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, addr, &dst.sin_addr) <= 0) {
        fprintf(stderr, "[UDP] 잘못된 주소: %s\n", addr);
        return -1;
    }

    n = sendto(fd, tx_buf, total_len, 0,
               (struct sockaddr *)&dst, sizeof(dst));

    if (n < 0) {
        fprintf(stderr, "[UDP] sendto 실패: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * @brief UDP 소켓에서 sentinel 패킷 수신
 *
 * @param fd       UDP 소켓 fd
 * @param pkt      [출력] 수신된 패킷
 * @param src_addr [출력] 발신자 주소 문자열 (NULL 이면 무시)
 * @return 0 성공, -1 실패
 */
int udp_recv_packet(int fd, sentinel_packet_t *pkt, char *src_addr)
{
    static uint8_t     rx_buf[SENTINEL_MAX_PACKET_SIZE];
    struct sockaddr_in src;
    socklen_t          src_len;
    ssize_t            n;

    if (fd < 0 || pkt == NULL) {
        return -1;
    }

    src_len = sizeof(src);
    n = recvfrom(fd, rx_buf, sizeof(rx_buf), 0,
                 (struct sockaddr *)&src, &src_len);

    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "[UDP] recvfrom 실패: %s\n", strerror(errno));
        }
        return -1;
    }

    if (src_addr != NULL) {
        (void)inet_ntop(AF_INET, &src.sin_addr, src_addr, 64);
    }

    return sentinel_packet_deserialize(rx_buf, (size_t)n, pkt);
}

/**
 * @brief UDP 멀티캐스트 그룹 참여
 *
 * 224.0.0.0 ~ 239.255.255.255 범위의 멀티캐스트 주소를 사용합니다.
 * 기본 멀티캐스트 그룹: 239.255.0.1 (로컬 스코프 관리용 주소 범위)
 *
 * @param fd    UDP 소켓 fd
 * @param group 멀티캐스트 그룹 주소 (예: "239.255.0.1")
 * @return 0 성공, -1 실패
 */
int udp_join_multicast(int fd, const char *group)
{
    struct ip_mreq mreq;

    assert(group != NULL);

    (void)memset(&mreq, 0, sizeof(mreq));

    if (inet_pton(AF_INET, group, &mreq.imr_multiaddr) <= 0) {
        fprintf(stderr, "[UDP] 잘못된 멀티캐스트 주소: %s\n", group);
        return -1;
    }

    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &mreq, sizeof(mreq)) != 0) {
        perror("[UDP] 멀티캐스트 그룹 참여 실패");
        return -1;
    }

    printf("[UDP] 멀티캐스트 그룹 참여: %s\n", group);
    return 0;
}
