/**
 * @file reporter.h
 * @brief 패킷 통계 집계 및 CSV 리포트 생성기
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_REPORTER_H
#define SENTINEL_REPORTER_H

#include "parser.h"
#include "rule_engine.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    uint64_t total_packets;
    uint64_t sentinel_packets;
    uint64_t anomaly_count;
    uint64_t total_bytes;
    uint64_t tcp_packets;
    uint64_t udp_packets;
    uint64_t other_packets;
    /* 노드별 통계 (인덱스 = node_id) */
    uint64_t node_packets[256];
    /* 토픽별 통계 (토픽 0x0001~0x0004만 사용) */
    uint64_t topic_counts[5];
    time_t   start_time;
    uint64_t pps_last_sec;      /* 직전 1초 처리 패킷 수 */
    time_t   pps_window_start;
} report_stats_t;

typedef struct {
    report_stats_t stats;
    FILE          *csv_file;
    char           csv_path[256];
    bool           csv_open;
    bool           tui_enabled;
    time_t         last_summary_time;
} reporter_t;

int  reporter_init(reporter_t *rep, const char *csv_path, bool tui);
void reporter_update(reporter_t *rep, const parsed_packet_t *pkt);
void reporter_alert(reporter_t *rep, const rule_alert_t *alert);
void reporter_print_summary(const reporter_t *rep);
void reporter_tui_refresh(reporter_t *rep);
void reporter_close(reporter_t *rep);

#endif /* SENTINEL_REPORTER_H */
