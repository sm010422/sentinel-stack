/**
 * @file rule_engine.h
 * @brief 룰 기반 이상 트래픽 탐지 엔진
 *
 * Snort IDS 문법을 참고하여 설계한 탐지 룰 엔진입니다.
 * 슬라이딩 윈도우 기반 임계치 탐지를 지원합니다.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_RULE_ENGINE_H
#define SENTINEL_RULE_ENGINE_H

#include "parser.h"
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

#define RULE_MAX_COUNT      64U
#define RULE_MAX_MSG_LEN    128U

typedef enum {
    RULE_PROTO_ANY,
    RULE_PROTO_TCP,
    RULE_PROTO_UDP,
    RULE_PROTO_SENTINEL
} rule_proto_t;

typedef enum {
    RULE_ACTION_ALERT,
    RULE_ACTION_LOG
} rule_action_t;

typedef enum {
    RULE_COND_SYN_FLOOD,
    RULE_COND_PORT_SCAN,
    RULE_COND_HIGH_PACKET_RATE,
    RULE_COND_INVALID_MAGIC,
    RULE_COND_RETRANSMIT_STORM,
    RULE_COND_LARGE_PAYLOAD
} rule_condition_t;

typedef struct {
    uint32_t          rule_id;
    char              msg[RULE_MAX_MSG_LEN];
    rule_proto_t      proto;
    rule_action_t     action;
    rule_condition_t  condition;
    uint32_t          threshold;
    uint32_t          window_sec;
    bool              enabled;
    uint64_t          trigger_count;
    /* 슬라이딩 윈도우 (초 단위 버킷 배열) */
    uint64_t          bucket_counts[8];
    uint64_t          bucket_start_sec;
} rule_t;

typedef struct {
    rule_t   rules[RULE_MAX_COUNT];
    uint32_t rule_count;
    uint64_t total_alerts;
} rule_engine_t;

typedef struct {
    uint32_t      rule_id;
    char          msg[RULE_MAX_MSG_LEN];
    rule_action_t action;
    struct timeval timestamp;
    uint32_t      src_ip;
    uint16_t      src_port;
    uint32_t      dst_ip;
    uint16_t      dst_port;
} rule_alert_t;

typedef void (*alert_cb_t)(const rule_alert_t *alert, void *userdata);

int  rule_engine_init(rule_engine_t *eng);
int  rule_engine_load_defaults(rule_engine_t *eng);
int  rule_engine_evaluate(rule_engine_t *eng, const parsed_packet_t *pkt,
                           alert_cb_t on_alert, void *userdata);
void rule_engine_print_stats(const rule_engine_t *eng);

#endif /* SENTINEL_RULE_ENGINE_H */
