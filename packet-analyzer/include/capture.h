/**
 * @file capture.h
 * @brief libpcap 기반 실시간 패킷 캡처 인터페이스
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_CAPTURE_H
#define SENTINEL_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pcap.h>

#define CAPTURE_SNAPLEN      65535U
#define CAPTURE_TIMEOUT_MS   100
#define CAPTURE_PROMISC      1

typedef struct {
    pcap_t  *handle;
    char     iface[64];
    char     filter_expr[256];
    bool     promiscuous;
    bool     running;
    uint64_t packets_captured;
    uint64_t packets_dropped;
    uint64_t bytes_captured;
} capture_ctx_t;

typedef void (*packet_handler_t)(const struct pcap_pkthdr *header,
                                  const uint8_t *packet,
                                  void *userdata);

int  capture_init(capture_ctx_t *ctx, const char *iface, const char *filter);
int  capture_open_offline(capture_ctx_t *ctx, const char *filename);
int  capture_set_filter(capture_ctx_t *ctx, const char *filter);
int  capture_start(capture_ctx_t *ctx, packet_handler_t handler, void *userdata);
void capture_stop(capture_ctx_t *ctx);
void capture_close(capture_ctx_t *ctx);
void capture_print_stats(const capture_ctx_t *ctx);

#endif /* SENTINEL_CAPTURE_H */
