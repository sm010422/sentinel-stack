/**
 * @file tcp_transport.h
 * @brief TCP 신뢰 채널 전송 레이어 인터페이스
 *
 * MIL-STD-1553 이중화 버스의 Primary Bus(신뢰 채널)에 해당합니다.
 * 연결 지향, 재전송 보장, 순서 보장 채널입니다.
 */

#ifndef SENTINEL_TCP_TRANSPORT_H
#define SENTINEL_TCP_TRANSPORT_H

#include "protocol.h"
#include <stdint.h>

int  tcp_create_server(uint32_t port);
int  tcp_accept(int server_fd, char *remote_addr);
int  tcp_connect(const char *addr, uint32_t port);
int  tcp_send_packet(int fd, const sentinel_packet_t *pkt);
int  tcp_recv_packet(int fd, sentinel_packet_t *pkt);
void tcp_close(int fd);

#endif /* SENTINEL_TCP_TRANSPORT_H */
