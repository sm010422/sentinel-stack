/**
 * @file udp_transport.h
 * @brief UDP 고속 채널 전송 레이어 인터페이스
 *
 * MIL-STD-1553 이중화 버스의 Redundant Bus(고속 채널)에 해당합니다.
 * 비연결형, 고속, 멀티캐스트 지원 채널입니다.
 */

#ifndef SENTINEL_UDP_TRANSPORT_H
#define SENTINEL_UDP_TRANSPORT_H

#include "protocol.h"
#include <stdint.h>

int udp_create_socket(uint32_t port);
int udp_send_packet(int fd, const char *addr, uint32_t port,
                    const sentinel_packet_t *pkt);
int udp_recv_packet(int fd, sentinel_packet_t *pkt, char *src_addr);
int udp_join_multicast(int fd, const char *group);

#endif /* SENTINEL_UDP_TRANSPORT_H */
