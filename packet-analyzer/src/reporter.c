/**
 * @file reporter.c
 * @brief 패킷 통계 집계 및 CSV 리포트 구현
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "reporter.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* =========================================================================
 * 초기화
 * ========================================================================= */

int reporter_init(reporter_t *rep, const char *csv_path, bool tui)
{
    if (rep == NULL) {
        return -1;
    }

    (void)memset(rep, 0, sizeof(reporter_t));
    rep->stats.start_time    = time(NULL);
    rep->stats.pps_window_start = rep->stats.start_time;
    rep->tui_enabled         = tui;
    rep->last_summary_time   = rep->stats.start_time;

    if (csv_path != NULL && csv_path[0] != '\0') {
        (void)snprintf(rep->csv_path, sizeof(rep->csv_path), "%s", csv_path);
        rep->csv_file = fopen(csv_path, "w");
        if (rep->csv_file != NULL) {
            rep->csv_open = true;
            /* CSV 헤더 */
            fprintf(rep->csv_file,
                    "timestamp,src_ip,dst_ip,src_port,dst_port,"
                    "proto,len,sentinel,topic,anomaly\n");
            (void)fflush(rep->csv_file);
        } else {
            fprintf(stderr, "[REPORTER] CSV 파일 열기 실패: %s\n", csv_path);
        }
    }

    printf("[REPORTER] 초기화 완료 (CSV: %s, TUI: %s)\n",
           (rep->csv_open) ? rep->csv_path : "비활성",
           tui ? "ON" : "OFF");

    return 0;
}

/* =========================================================================
 * 통계 업데이트
 * ========================================================================= */

void reporter_update(reporter_t *rep, const parsed_packet_t *pkt)
{
    char     src_ip[16];
    char     dst_ip[16];
    uint16_t topic_idx;
    time_t   now;

    if (rep == NULL || pkt == NULL) {
        return;
    }

    rep->stats.total_packets++;
    rep->stats.total_bytes += pkt->raw_len;

    if (pkt->has_tcp) {
        rep->stats.tcp_packets++;
    } else if (pkt->has_udp) {
        rep->stats.udp_packets++;
    } else {
        rep->stats.other_packets++;
    }

    if (pkt->has_sentinel) {
        rep->stats.sentinel_packets++;

        /* 노드별 집계 */
        rep->stats.node_packets[pkt->sentinel.src_node_id]++;

        /* 토픽별 집계 (0x0001~0x0004 → index 1~4) */
        topic_idx = pkt->sentinel.topic_id;
        if (topic_idx >= 1U && topic_idx <= 4U) {
            rep->stats.topic_counts[topic_idx]++;
        }
    }

    /* PPS 카운트 */
    now = time(NULL);
    if (now > rep->stats.pps_window_start) {
        rep->stats.pps_last_sec    = 0U;
        rep->stats.pps_window_start = now;
    }
    rep->stats.pps_last_sec++;

    /* CSV 기록 */
    if (rep->csv_open && pkt->has_ip) {
        ip_to_str(pkt->ip.src_ip, src_ip, sizeof(src_ip));
        ip_to_str(pkt->ip.dst_ip, dst_ip, sizeof(dst_ip));

        fprintf(rep->csv_file,
                "%ld,%s,%s,%u,%u,%s,%u,%s,%s,0\n",
                (long)pkt->timestamp.tv_sec,
                src_ip, dst_ip,
                pkt->has_tcp ? pkt->tcp.src_port : pkt->udp.src_port,
                pkt->has_tcp ? pkt->tcp.dst_port : pkt->udp.dst_port,
                pkt->has_tcp ? "TCP" : "UDP",
                pkt->raw_len,
                pkt->has_sentinel ? "YES" : "NO",
                pkt->has_sentinel
                    ? sentinel_topic_name(pkt->sentinel.topic_id)
                    : "N/A");
    }
}

void reporter_alert(reporter_t *rep, const rule_alert_t *alert)
{
    char src_ip[16];
    char dst_ip[16];

    if (rep == NULL || alert == NULL) {
        return;
    }

    rep->stats.anomaly_count++;

    if (rep->csv_open) {
        ip_to_str(alert->src_ip, src_ip, sizeof(src_ip));
        ip_to_str(alert->dst_ip, dst_ip, sizeof(dst_ip));

        fprintf(rep->csv_file,
                "%ld,%s,%s,%u,%u,ALERT,0,NO,N/A,%u:%s\n",
                (long)alert->timestamp.tv_sec,
                src_ip, dst_ip,
                alert->src_port, alert->dst_port,
                alert->rule_id, alert->msg);
        (void)fflush(rep->csv_file);
    }
}

void reporter_print_summary(const reporter_t *rep)
{
    time_t  elapsed;
    double  sentinel_ratio;

    if (rep == NULL) {
        return;
    }

    elapsed = time(NULL) - rep->stats.start_time;
    if (elapsed == 0) elapsed = 1;

    sentinel_ratio = (rep->stats.total_packets > 0U)
                     ? (double)rep->stats.sentinel_packets
                       / (double)rep->stats.total_packets * 100.0
                     : 0.0;

    printf("\n=== %llds 집계 ===\n", (long long)elapsed);
    printf("총 패킷    : %llu\n",
           (unsigned long long)rep->stats.total_packets);
    printf("Sentinel   : %llu (%.1f%%)\n",
           (unsigned long long)rep->stats.sentinel_packets,
           sentinel_ratio);
    printf("이상 탐지  : %llu\n",
           (unsigned long long)rep->stats.anomaly_count);
    printf("평균 크기  : %.1f bytes\n",
           (rep->stats.total_packets > 0U)
             ? (double)rep->stats.total_bytes / (double)rep->stats.total_packets
             : 0.0);
    printf("TCP        : %llu  UDP: %llu\n",
           (unsigned long long)rep->stats.tcp_packets,
           (unsigned long long)rep->stats.udp_packets);
    printf("현재 PPS   : %llu\n",
           (unsigned long long)rep->stats.pps_last_sec);
    printf("==================\n\n");
}

void reporter_tui_refresh(reporter_t *rep)
{
    /* ncurses 미사용 버전: 일반 출력으로 대체 */
    if (rep == NULL) {
        return;
    }
    reporter_print_summary(rep);
}

void reporter_close(reporter_t *rep)
{
    if (rep == NULL) {
        return;
    }

    if (rep->csv_open && rep->csv_file != NULL) {
        (void)fclose(rep->csv_file);
        rep->csv_file = NULL;
        rep->csv_open = false;
        printf("[REPORTER] CSV 저장 완료: %s\n", rep->csv_path);
    }
}
