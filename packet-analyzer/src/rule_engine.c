/**
 * @file rule_engine.c
 * @brief 룰 기반 이상 탐지 엔진 구현
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "rule_engine.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

/* =========================================================================
 * 슬라이딩 윈도우 헬퍼
 * ========================================================================= */

#define BUCKET_COUNT 8U

/**
 * @brief 현재 초에 해당하는 버킷 인덱스를 반환하고 오래된 버킷을 초기화
 */
static uint64_t window_add(rule_t *r, uint64_t now_sec)
{
    uint64_t window_sec;
    uint64_t elapsed;
    uint32_t idx;
    uint32_t i;
    uint64_t total;

    window_sec = r->window_sec > 0U ? r->window_sec : 1U;

    if (r->bucket_start_sec == 0U) {
        r->bucket_start_sec = now_sec;
        (void)memset(r->bucket_counts, 0, sizeof(r->bucket_counts));
    }

    elapsed = now_sec - r->bucket_start_sec;

    /* 윈도우를 벗어난 경우 오래된 버킷 초기화 */
    if (elapsed >= BUCKET_COUNT) {
        /* 전부 초기화 */
        (void)memset(r->bucket_counts, 0, sizeof(r->bucket_counts));
        r->bucket_start_sec = now_sec;
        elapsed = 0U;
    }

    idx = (uint32_t)(now_sec % BUCKET_COUNT);
    r->bucket_counts[idx]++;

    /* 윈도우 내 총합 계산 */
    total = 0U;
    for (i = 0U; i < BUCKET_COUNT; i++) {
        uint64_t bucket_time = r->bucket_start_sec + i;
        if ((now_sec >= bucket_time) &&
            (now_sec - bucket_time < window_sec)) {
            total += r->bucket_counts[i];
        }
    }

    (void)elapsed;
    return total;
}

/* =========================================================================
 * 알림 발생
 * ========================================================================= */

static void fire_alert(const rule_t *r, const parsed_packet_t *pkt,
                        alert_cb_t cb, void *userdata)
{
    rule_alert_t alert;

    if (cb == NULL) {
        return;
    }

    (void)memset(&alert, 0, sizeof(alert));
    alert.rule_id = r->rule_id;
    (void)snprintf(alert.msg, sizeof(alert.msg), "%s", r->msg);
    alert.action    = r->action;
    alert.timestamp = pkt->timestamp;

    if (pkt->has_ip) {
        alert.src_ip = pkt->ip.src_ip;
        alert.dst_ip = pkt->ip.dst_ip;
    }
    if (pkt->has_tcp) {
        alert.src_port = pkt->tcp.src_port;
        alert.dst_port = pkt->tcp.dst_port;
    } else if (pkt->has_udp) {
        alert.src_port = pkt->udp.src_port;
        alert.dst_port = pkt->udp.dst_port;
    }

    cb(&alert, userdata);
}

/* =========================================================================
 * API 구현
 * ========================================================================= */

int rule_engine_init(rule_engine_t *eng)
{
    if (eng == NULL) {
        return -1;
    }

    (void)memset(eng, 0, sizeof(rule_engine_t));
    return 0;
}

int rule_engine_load_defaults(rule_engine_t *eng)
{
    rule_t *r;

    if (eng == NULL) {
        return -1;
    }

    /* ── 규칙 1: SYN Flood ───────────────────────────────── */
    r = &eng->rules[eng->rule_count++];
    r->rule_id   = 1001U;
    r->action    = RULE_ACTION_ALERT;
    r->proto     = RULE_PROTO_TCP;
    r->condition = RULE_COND_SYN_FLOOD;
    r->threshold = 20U;
    r->window_sec = 1U;
    r->enabled   = true;
    (void)snprintf(r->msg, sizeof(r->msg), "SYN Flood Detected");

    /* ── 규칙 2: Port Scan ───────────────────────────────── */
    r = &eng->rules[eng->rule_count++];
    r->rule_id   = 1002U;
    r->action    = RULE_ACTION_ALERT;
    r->proto     = RULE_PROTO_TCP;
    r->condition = RULE_COND_PORT_SCAN;
    r->threshold = 15U;
    r->window_sec = 5U;
    r->enabled   = true;
    (void)snprintf(r->msg, sizeof(r->msg), "Port Scan Detected");

    /* ── 규칙 3: High Packet Rate ────────────────────────── */
    r = &eng->rules[eng->rule_count++];
    r->rule_id   = 1003U;
    r->action    = RULE_ACTION_ALERT;
    r->proto     = RULE_PROTO_ANY;
    r->condition = RULE_COND_HIGH_PACKET_RATE;
    r->threshold = 10000U;
    r->window_sec = 1U;
    r->enabled   = true;
    (void)snprintf(r->msg, sizeof(r->msg), "High Packet Rate Anomaly");

    /* ── 규칙 4: Invalid Magic ───────────────────────────── */
    r = &eng->rules[eng->rule_count++];
    r->rule_id   = 2002U;
    r->action    = RULE_ACTION_ALERT;
    r->proto     = RULE_PROTO_TCP;
    r->condition = RULE_COND_INVALID_MAGIC;
    r->threshold = 1U;
    r->window_sec = 1U;
    r->enabled   = true;
    (void)snprintf(r->msg, sizeof(r->msg),
                   "Invalid Magic Number - Possible Protocol Violation");

    /* ── 규칙 5: sentinel Retransmit Storm ───────────────── */
    r = &eng->rules[eng->rule_count++];
    r->rule_id   = 2001U;
    r->action    = RULE_ACTION_ALERT;
    r->proto     = RULE_PROTO_SENTINEL;
    r->condition = RULE_COND_RETRANSMIT_STORM;
    r->threshold = 10U;
    r->window_sec = 5U;
    r->enabled   = true;
    (void)snprintf(r->msg, sizeof(r->msg), "Sentinel Retransmit Storm");

    printf("[RULE] 기본 룰 %u개 로드 완료\n", eng->rule_count);
    return 0;
}

int rule_engine_evaluate(rule_engine_t *eng, const parsed_packet_t *pkt,
                          alert_cb_t on_alert, void *userdata)
{
    uint32_t i;
    uint64_t now_sec;
    uint64_t window_total;

    if (eng == NULL || pkt == NULL) {
        return -1;
    }

    now_sec = (uint64_t)pkt->timestamp.tv_sec;
    if (now_sec == 0U) {
        now_sec = (uint64_t)time(NULL);
    }

    for (i = 0U; i < eng->rule_count; i++) {
        rule_t *r = &eng->rules[i];

        if (!r->enabled) {
            continue;
        }

        /* 프로토콜 필터 */
        if (r->proto == RULE_PROTO_TCP && !pkt->has_tcp) continue;
        if (r->proto == RULE_PROTO_UDP && !pkt->has_udp) continue;
        if (r->proto == RULE_PROTO_SENTINEL && !pkt->has_sentinel) continue;

        window_total = 0U;

        switch (r->condition) {

        case RULE_COND_SYN_FLOOD:
            /* TCP SYN 패킷 카운트 */
            if (pkt->has_tcp && (pkt->tcp.flags & TCP_FLAG_SYN) != 0U &&
                (pkt->tcp.flags & TCP_FLAG_ACK) == 0U) {
                window_total = window_add(r, now_sec);
                if (window_total > (uint64_t)r->threshold) {
                    r->trigger_count++;
                    eng->total_alerts++;
                    fire_alert(r, pkt, on_alert, userdata);
                }
            }
            break;

        case RULE_COND_PORT_SCAN:
            /* 단순화: SYN 패킷을 포트 스캔 신호로 간주 */
            if (pkt->has_tcp && (pkt->tcp.flags & TCP_FLAG_SYN) != 0U) {
                window_total = window_add(r, now_sec);
                if (window_total > (uint64_t)r->threshold) {
                    r->trigger_count++;
                    eng->total_alerts++;
                    fire_alert(r, pkt, on_alert, userdata);
                }
            }
            break;

        case RULE_COND_HIGH_PACKET_RATE:
            window_total = window_add(r, now_sec);
            if (window_total > (uint64_t)r->threshold) {
                r->trigger_count++;
                eng->total_alerts++;
                fire_alert(r, pkt, on_alert, userdata);
            }
            break;

        case RULE_COND_INVALID_MAGIC:
            /* 포트 9000 TCP이지만 sentinel 파싱 실패한 경우 */
            if (pkt->has_tcp &&
                (pkt->tcp.dst_port == 9000U || pkt->tcp.src_port == 9000U) &&
                !pkt->has_sentinel) {
                r->trigger_count++;
                eng->total_alerts++;
                fire_alert(r, pkt, on_alert, userdata);
            }
            break;

        case RULE_COND_RETRANSMIT_STORM:
            /* sentinel 재전송: 동일 seq_num 반복 (단순화) */
            if (pkt->has_sentinel) {
                window_total = window_add(r, now_sec);
                if (window_total > (uint64_t)r->threshold) {
                    r->trigger_count++;
                    eng->total_alerts++;
                    fire_alert(r, pkt, on_alert, userdata);
                }
            }
            break;

        case RULE_COND_LARGE_PAYLOAD:
            if (pkt->payload_len > r->threshold) {
                r->trigger_count++;
                eng->total_alerts++;
                fire_alert(r, pkt, on_alert, userdata);
            }
            break;

        default:
            break;
        }
    }

    return 0;
}

void rule_engine_print_stats(const rule_engine_t *eng)
{
    uint32_t i;

    if (eng == NULL) {
        return;
    }

    printf("\n[RULE] ===== 탐지 룰 통계 =====\n");
    printf("       총 경보 수: %llu\n",
           (unsigned long long)eng->total_alerts);
    printf("\n       %-8s %-40s %8s\n", "규칙 ID", "메시지", "경보 횟수");

    for (i = 0U; i < eng->rule_count; i++) {
        const rule_t *r = &eng->rules[i];
        printf("       %-8u %-40s %8llu\n",
               r->rule_id, r->msg,
               (unsigned long long)r->trigger_count);
    }

    printf("[RULE] ========================\n\n");
}
