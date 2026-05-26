/**
 * @file comm_node.c
 * @brief sentinel 통신 노드 이벤트 루프 구현 및 독립 실행 진입점
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "comm_node.h"
#include "protocol.h"
#include "crypto.h"
#include "pubsub.h"
#include "io_multiplexer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <time.h>

/* TCP 전송 함수 전방 선언 */
extern int  tcp_create_server(uint32_t port);
extern int  tcp_accept(int server_fd, char *remote_addr);
extern int  tcp_connect(const char *addr, uint32_t port);
extern int  tcp_send_packet(int fd, const sentinel_packet_t *pkt);
extern int  tcp_recv_packet(int fd, sentinel_packet_t *pkt);
extern void tcp_close(int fd);
extern int  udp_create_socket(uint32_t port);
extern int  udp_send_packet(int fd, const char *addr, uint32_t port,
                             const sentinel_packet_t *pkt);
extern int  udp_recv_packet(int fd, sentinel_packet_t *pkt, char *src_addr);

/* =========================================================================
 * 데모용 AES-256 키 (실제 배포 시 외부 KDP로 교체 필수)
 * ========================================================================= */

static const uint8_t DEMO_AES_KEY[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

/* =========================================================================
 * 런타임 종료 플래그
 * ========================================================================= */

static volatile int g_stop = 0;

static void handle_sigint(int signum)
{
    (void)signum;
    g_stop = 1;
    printf("\n[NODE] SIGINT — 종료 중...\n");
}

/* =========================================================================
 * API 구현
 * ========================================================================= */

const char *node_role_str(node_role_t role)
{
    switch (role) {
        case NODE_ROLE_COMMANDER: return "commander";
        case NODE_ROLE_SENSOR:    return "sensor";
        case NODE_ROLE_RELAY:     return "relay";
        default:                  return "unknown";
    }
}

int comm_node_init(comm_node_t *node, uint8_t id, node_role_t role,
                   uint32_t tcp_port, uint32_t udp_port)
{
    if (node == NULL || id == SENTINEL_NODE_BROADCAST) {
        return -1;
    }

    (void)memset(node, 0, sizeof(comm_node_t));

    node->node_id        = id;
    node->role           = role;
    node->tcp_port       = (tcp_port > 0U) ? tcp_port : SENTINEL_PORT_TCP;
    node->udp_port       = (udp_port > 0U) ? udp_port : SENTINEL_PORT_UDP;
    node->tcp_fd         = -1;
    node->udp_fd         = -1;
    node->tcp_server_fd  = -1;
    node->connected      = false;
    node->running        = false;

    (void)snprintf(node->name, sizeof(node->name),
                   "%s_%u", node_role_str(role), id);

    /* 암호화 초기화 (데모 키) */
    if (crypto_init(&node->crypto, DEMO_AES_KEY, sizeof(DEMO_AES_KEY)) != 0) {
        fprintf(stderr, "[NODE] 암호화 초기화 실패\n");
        return -1;
    }

    /* PubSub 초기화 */
    if (pubsub_init(&node->pubsub) != 0) {
        return -1;
    }

    /* 기본 토픽 등록 */
    (void)pubsub_register_topic(&node->pubsub, SENTINEL_TOPIC_SENSOR_DATA);
    (void)pubsub_register_topic(&node->pubsub, SENTINEL_TOPIC_STATUS);
    (void)pubsub_register_topic(&node->pubsub, SENTINEL_TOPIC_CMD);
    (void)pubsub_register_topic(&node->pubsub, SENTINEL_TOPIC_HEARTBEAT);

    /* I/O 멀티플렉서 초기화 */
    if (io_init(&node->io) != 0) {
        return -1;
    }

    /* UDP 소켓 생성 */
    node->udp_fd = udp_create_socket(node->udp_port);
    if (node->udp_fd < 0) {
        fprintf(stderr, "[NODE] UDP 소켓 생성 실패\n");
        return -1;
    }

    (void)signal(SIGINT, handle_sigint);

    printf("[NODE] 초기화 완료: id=%u role=%s tcp=%u udp=%u\n",
           id, node_role_str(role), node->tcp_port, node->udp_port);

    return 0;
}

int comm_node_listen(comm_node_t *node)
{
    if (node == NULL) {
        return -1;
    }

    node->tcp_server_fd = tcp_create_server(node->tcp_port);
    if (node->tcp_server_fd < 0) {
        return -1;
    }

    (void)io_add_fd(&node->io, node->tcp_server_fd,
                    IO_EVENT_READ, NULL);
    (void)io_add_fd(&node->io, node->udp_fd,
                    IO_EVENT_READ, NULL);

    printf("[NODE] TCP 서버 listen 중: 포트 %u\n", node->tcp_port);
    return 0;
}

int comm_node_connect(comm_node_t *node, const char *addr, uint32_t port)
{
    if (node == NULL || addr == NULL) {
        return -1;
    }

    (void)snprintf(node->peer_addr, sizeof(node->peer_addr), "%s", addr);
    node->peer_tcp_port = (port > 0U) ? port : SENTINEL_PORT_TCP;

    node->tcp_fd = tcp_connect(addr, node->peer_tcp_port);
    if (node->tcp_fd < 0) {
        return -1;
    }

    node->connected = true;

    (void)io_add_fd(&node->io, node->tcp_fd, IO_EVENT_READ, NULL);
    (void)io_add_fd(&node->io, node->udp_fd, IO_EVENT_READ, NULL);

    return 0;
}

int comm_node_publish(comm_node_t *node, uint16_t topic_id,
                      const uint8_t *data, size_t len)
{
    sentinel_packet_t pkt;
    uint8_t           plaintext[SENTINEL_MAX_PAYLOAD];
    uint8_t           iv[CRYPTO_IV_LEN];
    size_t            ct_len;

    if (node == NULL || data == NULL || len == 0U) {
        return -1;
    }
    if (len > SENTINEL_MAX_PAYLOAD) {
        fprintf(stderr, "[NODE] 페이로드 크기 초과: %zu\n", len);
        return -1;
    }

    (void)memset(&pkt, 0, sizeof(pkt));

    /* 헤더 구성 */
    pkt.header.version      = SENTINEL_VERSION;
    pkt.header.msg_type     = (uint8_t)SENTINEL_MSG_DATA;
    pkt.header.seq_num      = node->seq_num++;
    pkt.header.timestamp_ms = (uint64_t)time(NULL) * 1000ULL;
    pkt.header.src_node_id  = node->node_id;
    pkt.header.dst_node_id  = SENTINEL_NODE_BROADCAST;
    pkt.header.topic_id     = topic_id;

    /* IV 생성 */
    if (crypto_generate_iv(iv) != 0) {
        return -1;
    }
    (void)memcpy(pkt.header.aes_iv, iv, CRYPTO_IV_LEN);

    /* 페이로드를 정적 버퍼에 복사 후 암호화 */
    (void)memcpy(plaintext, data, len);
    ct_len = 0U;

    if (crypto_encrypt(&node->crypto,
                       plaintext, len,
                       iv,
                       pkt.enc_payload, &ct_len,
                       pkt.aes_tag) != 0) {
        fprintf(stderr, "[NODE] 암호화 실패\n");
        return -1;
    }

    pkt.enc_payload_len   = (uint32_t)ct_len;
    pkt.header.payload_len = (uint32_t)ct_len;

    /* TCP 전송 */
    if (node->connected && node->tcp_fd >= 0) {
        if (tcp_send_packet(node->tcp_fd, &pkt) == 0) {
            node->packets_sent++;
            node->bytes_sent += SENTINEL_HEADER_SIZE + ct_len
                                + SENTINEL_AES_TAG_LEN + 4U;
        }
    }

    /* 로컬 PubSub 발행 (로컬 구독자에게 전달) */
    (void)pubsub_publish(&node->pubsub, topic_id, data, len);

    return 0;
}

int comm_node_run(comm_node_t *node)
{
    io_event_t      events[IO_MAX_EVENTS];
    sentinel_packet_t rx_pkt;
    uint8_t         plaintext[SENTINEL_MAX_PAYLOAD];
    size_t          pt_len;
    uint64_t        last_hb_ms;
    int             n;
    int             i;
    time_t          now_sec;

    if (node == NULL) {
        return -1;
    }

    node->running = true;
    g_stop        = 0;
    last_hb_ms    = 0U;

    printf("[NODE] 이벤트 루프 시작: %s (id=%u)\n",
           node->name, node->node_id);

    while (node->running && (g_stop == 0)) {
        n = io_wait(&node->io, events, IO_MAX_EVENTS,
                    (int)COMM_HEARTBEAT_MS);

        if (n < 0 && g_stop == 0) {
            continue;
        }

        /* ── I/O 이벤트 처리 ─────────────────────────── */
        for (i = 0; i < n; i++) {
            int fd = events[i].fd;

            /* TCP 서버: 새 연결 수락 */
            if (fd == node->tcp_server_fd) {
                char remote[64] = {0};
                int  client_fd  = tcp_accept(fd, remote);
                if (client_fd >= 0) {
                    if (node->tcp_fd >= 0) {
                        tcp_close(node->tcp_fd); /* 기존 연결 대체 */
                    }
                    node->tcp_fd    = client_fd;
                    node->connected = true;
                    (void)io_add_fd(&node->io, client_fd, IO_EVENT_READ, NULL);
                }
                continue;
            }

            /* TCP 클라이언트: 패킷 수신 */
            if (fd == node->tcp_fd) {
                if (tcp_recv_packet(fd, &rx_pkt) == 0) {
                    node->packets_recv++;

                    /* 복호화 */
                    pt_len = 0U;
                    if (crypto_decrypt(&node->crypto,
                                       rx_pkt.enc_payload,
                                       rx_pkt.enc_payload_len,
                                       rx_pkt.header.aes_iv,
                                       rx_pkt.aes_tag,
                                       plaintext, &pt_len) == 0) {
                        /* PubSub으로 로컬 전달 */
                        (void)pubsub_publish(&node->pubsub,
                                             rx_pkt.header.topic_id,
                                             plaintext, pt_len);
                    } else {
                        node->decrypt_fail_count++;
                    }
                } else {
                    /* 연결 끊김 — 재연결 시도 */
                    printf("[NODE] TCP 연결 끊김 — 재연결 예약\n");
                    (void)io_remove_fd(&node->io, fd);
                    tcp_close(fd);
                    node->tcp_fd    = -1;
                    node->connected = false;
                    node->reconnect_count++;
                }
                continue;
            }

            /* UDP: 패킷 수신 */
            if (fd == node->udp_fd) {
                if (udp_recv_packet(fd, &rx_pkt, NULL) == 0) {
                    node->packets_recv++;
                    pt_len = 0U;
                    if (crypto_decrypt(&node->crypto,
                                       rx_pkt.enc_payload,
                                       rx_pkt.enc_payload_len,
                                       rx_pkt.header.aes_iv,
                                       rx_pkt.aes_tag,
                                       plaintext, &pt_len) == 0) {
                        (void)pubsub_publish(&node->pubsub,
                                             rx_pkt.header.topic_id,
                                             plaintext, pt_len);
                    }
                }
                continue;
            }
        }

        /* ── Heartbeat 전송 (주기적) ─────────────────── */
        now_sec = time(NULL);
        if (((uint64_t)now_sec * 1000ULL) >= (last_hb_ms + COMM_HEARTBEAT_MS)) {
            static const uint8_t hb_data[] = {0x01};
            (void)comm_node_publish(node, SENTINEL_TOPIC_HEARTBEAT,
                                    hb_data, sizeof(hb_data));
            last_hb_ms = (uint64_t)now_sec * 1000ULL;
        }

        /* ── 재연결 시도 (TCP 연결 없을 때) ─────────── */
        if (!node->connected && (node->peer_addr[0] != '\0')) {
            node->tcp_fd = tcp_connect(node->peer_addr, node->peer_tcp_port);
            if (node->tcp_fd >= 0) {
                node->connected = true;
                node->reconnect_count++;
                (void)io_add_fd(&node->io, node->tcp_fd, IO_EVENT_READ, NULL);
                printf("[NODE] 재연결 성공 (#%llu)\n",
                       (unsigned long long)node->reconnect_count);
            }
        }
    }

    node->running = false;
    comm_node_print_stats(node);
    return 0;
}

void comm_node_stop(comm_node_t *node)
{
    if (node != NULL) {
        node->running = false;
    }
    g_stop = 1;
}

void comm_node_shutdown(comm_node_t *node)
{
    if (node == NULL) {
        return;
    }

    comm_node_stop(node);

    if (node->tcp_fd >= 0) {
        tcp_close(node->tcp_fd);
        node->tcp_fd = -1;
    }
    if (node->tcp_server_fd >= 0) {
        tcp_close(node->tcp_server_fd);
        node->tcp_server_fd = -1;
    }
    if (node->udp_fd >= 0) {
        (void)close(node->udp_fd);
        node->udp_fd = -1;
    }

    io_close(&node->io);
    crypto_clear(&node->crypto);

    printf("[NODE] 종료 완료: %s\n", node->name);
}

void comm_node_print_stats(const comm_node_t *node)
{
    if (node == NULL) {
        return;
    }

    printf("\n[NODE] ===== 통계 보고서 =====\n");
    printf("       노드 ID    : %u (%s)\n", node->node_id, node->name);
    printf("       역할       : %s\n", node_role_str(node->role));
    printf("       전송 패킷  : %llu\n", (unsigned long long)node->packets_sent);
    printf("       수신 패킷  : %llu\n", (unsigned long long)node->packets_recv);
    printf("       전송 바이트: %llu\n", (unsigned long long)node->bytes_sent);
    printf("       수신 바이트: %llu\n", (unsigned long long)node->bytes_recv);
    printf("       재연결 횟수: %llu\n", (unsigned long long)node->reconnect_count);
    printf("       복호화 실패: %llu\n", (unsigned long long)node->decrypt_fail_count);
    printf("       CRC 오류  : %llu\n", (unsigned long long)node->crc_fail_count);
    printf("[NODE] ==========================\n\n");

    pubsub_print_stats(&node->pubsub);
}

/* =========================================================================
 * 독립 실행 모드
 * ========================================================================= */

#ifdef COMM_STANDALONE

static void on_sensor_data(uint16_t topic_id, const uint8_t *data,
                            size_t len, void *userdata)
{
    (void)userdata;
    printf("[SUBSCRIBER] 토픽=0x%04X len=%zu 데이터수신\n", topic_id, len);
}

/**
 * @brief comm_node 진입점
 *
 * 사용법:
 *   comm_node --id N --role commander|sensor --tcp-port P --udp-port Q
 *   comm_node --id N --role sensor --connect 127.0.0.1:9000 --subscribe SENSOR_DATA,STATUS
 */
int main(int argc, char *argv[])
{
    comm_node_t  node;
    uint8_t      node_id   = 1U;
    node_role_t  role      = NODE_ROLE_COMMANDER;
    uint32_t     tcp_port  = SENTINEL_PORT_TCP;
    uint32_t     udp_port  = SENTINEL_PORT_UDP;
    char         peer_addr[64] = {0};
    uint32_t     peer_port = 0U;
    int          i;

    for (i = 1; i < argc - 1; i++) {
        if      (strcmp(argv[i], "--id")       == 0) { node_id  = (uint8_t)atoi(argv[i+1]); }
        else if (strcmp(argv[i], "--tcp-port") == 0) { tcp_port = (uint32_t)atoi(argv[i+1]); }
        else if (strcmp(argv[i], "--udp-port") == 0) { udp_port = (uint32_t)atoi(argv[i+1]); }
        else if (strcmp(argv[i], "--role")     == 0) {
            if (strcmp(argv[i+1], "sensor") == 0) role = NODE_ROLE_SENSOR;
            else if (strcmp(argv[i+1], "relay") == 0) role = NODE_ROLE_RELAY;
        }
        else if (strcmp(argv[i], "--connect")  == 0) {
            /* "addr:port" 파싱 */
            char *colon = strchr(argv[i+1], ':');
            if (colon != NULL) {
                size_t alen = (size_t)(colon - argv[i+1]);
                if (alen < sizeof(peer_addr)) {
                    (void)memcpy(peer_addr, argv[i+1], alen);
                    peer_addr[alen] = '\0';
                }
                peer_port = (uint32_t)atoi(colon + 1);
            }
        }
    }

    if (comm_node_init(&node, node_id, role, tcp_port, udp_port) != 0) {
        fprintf(stderr, "노드 초기화 실패\n");
        return 1;
    }

    if (peer_addr[0] != '\0') {
        (void)comm_node_connect(&node, peer_addr, peer_port);
        (void)pubsub_subscribe(&node.pubsub, SENTINEL_TOPIC_SENSOR_DATA,
                               on_sensor_data, NULL);
    } else {
        (void)comm_node_listen(&node);
    }

    (void)comm_node_run(&node);
    comm_node_shutdown(&node);

    return 0;
}

#endif /* COMM_STANDALONE */
