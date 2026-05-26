/**
 * @file capture.c
 * @brief libpcap 패킷 캡처 구현
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "capture.h"
#include "parser.h"
#include "rule_engine.h"
#include "reporter.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>

/* =========================================================================
 * 전역 캡처 컨텍스트 (시그널 핸들러용)
 * ========================================================================= */

static capture_ctx_t *g_ctx = NULL;

static void handle_sigint(int signum)
{
    (void)signum;
    if (g_ctx != NULL) {
        g_ctx->running = false;
        pcap_breakloop(g_ctx->handle);
    }
    printf("\n[PCAP] 캡처 중단\n");
}

/* =========================================================================
 * API 구현
 * ========================================================================= */

int capture_init(capture_ctx_t *ctx, const char *iface, const char *filter)
{
    char errbuf[PCAP_ERRBUF_SIZE];

    if (ctx == NULL || iface == NULL) {
        return -1;
    }

    (void)memset(ctx, 0, sizeof(capture_ctx_t));
    (void)snprintf(ctx->iface, sizeof(ctx->iface), "%s", iface);

    ctx->handle = pcap_open_live(iface,
                                  CAPTURE_SNAPLEN,
                                  CAPTURE_PROMISC,
                                  CAPTURE_TIMEOUT_MS,
                                  errbuf);
    if (ctx->handle == NULL) {
        fprintf(stderr, "[PCAP] pcap_open_live 실패: %s\n", errbuf);
        return -1;
    }

    /* 링크 계층 타입 확인 (Ethernet만 지원) */
    if (pcap_datalink(ctx->handle) != DLT_EN10MB) {
        fprintf(stderr, "[PCAP] Ethernet이 아닌 링크 타입은 지원하지 않습니다.\n");
        pcap_close(ctx->handle);
        ctx->handle = NULL;
        return -1;
    }

    if (filter != NULL && filter[0] != '\0') {
        if (capture_set_filter(ctx, filter) != 0) {
            pcap_close(ctx->handle);
            ctx->handle = NULL;
            return -1;
        }
    }

    printf("[PCAP] 인터페이스 '%s' 열림 (무차별 모드: %s)\n",
           iface, CAPTURE_PROMISC ? "ON" : "OFF");

    return 0;
}

int capture_open_offline(capture_ctx_t *ctx, const char *filename)
{
    char errbuf[PCAP_ERRBUF_SIZE];

    if (ctx == NULL || filename == NULL) {
        return -1;
    }

    (void)memset(ctx, 0, sizeof(capture_ctx_t));
    (void)snprintf(ctx->iface, sizeof(ctx->iface), "file:%s", filename);

    ctx->handle = pcap_open_offline(filename, errbuf);
    if (ctx->handle == NULL) {
        fprintf(stderr, "[PCAP] 파일 열기 실패: %s\n", errbuf);
        return -1;
    }

    printf("[PCAP] 오프라인 파일 열림: %s\n", filename);
    return 0;
}

int capture_set_filter(capture_ctx_t *ctx, const char *filter)
{
    struct bpf_program bpf;
    bpf_u_int32        net = 0;
    bpf_u_int32        mask = 0;
    char               errbuf[PCAP_ERRBUF_SIZE];

    if (ctx == NULL || ctx->handle == NULL || filter == NULL) {
        return -1;
    }

    /* 네트워크 정보 조회 (BPF 필터 컴파일용) */
    if (ctx->iface[0] != 'f') { /* 파일이 아닌 경우 */
        (void)pcap_lookupnet(ctx->iface, &net, &mask, errbuf);
    }

    if (pcap_compile(ctx->handle, &bpf, filter, 1, net) != 0) {
        fprintf(stderr, "[PCAP] BPF 컴파일 실패: %s\n",
                pcap_geterr(ctx->handle));
        return -1;
    }

    if (pcap_setfilter(ctx->handle, &bpf) != 0) {
        fprintf(stderr, "[PCAP] BPF 설치 실패: %s\n",
                pcap_geterr(ctx->handle));
        pcap_freecode(&bpf);
        return -1;
    }

    pcap_freecode(&bpf);
    (void)snprintf(ctx->filter_expr, sizeof(ctx->filter_expr), "%s", filter);
    printf("[PCAP] BPF 필터 설정: '%s'\n", filter);
    return 0;
}

int capture_start(capture_ctx_t *ctx, packet_handler_t handler, void *userdata)
{
    struct pcap_pkthdr *header;
    const uint8_t      *data;
    int                 ret;

    if (ctx == NULL || ctx->handle == NULL || handler == NULL) {
        return -1;
    }

    g_ctx        = ctx;
    ctx->running = true;
    (void)signal(SIGINT, handle_sigint);

    printf("[PCAP] 캡처 시작 (인터페이스: %s)\n", ctx->iface);

    while (ctx->running) {
        ret = pcap_next_ex(ctx->handle, &header, &data);

        if (ret == 1) {
            /* 패킷 수신 성공 */
            ctx->packets_captured++;
            ctx->bytes_captured += header->caplen;
            handler(header, data, userdata);
        } else if (ret == 0) {
            /* 타임아웃 — 계속 */
            continue;
        } else if (ret == -2) {
            /* EOF (오프라인 파일) */
            break;
        } else {
            fprintf(stderr, "[PCAP] 오류: %s\n", pcap_geterr(ctx->handle));
            break;
        }
    }

    ctx->running = false;
    g_ctx        = NULL;

    capture_print_stats(ctx);
    return 0;
}

void capture_stop(capture_ctx_t *ctx)
{
    if (ctx != NULL) {
        ctx->running = false;
        if (ctx->handle != NULL) {
            pcap_breakloop(ctx->handle);
        }
    }
}

void capture_close(capture_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    capture_stop(ctx);
    if (ctx->handle != NULL) {
        pcap_close(ctx->handle);
        ctx->handle = NULL;
    }
}

void capture_print_stats(const capture_ctx_t *ctx)
{
    struct pcap_stat ps;

    if (ctx == NULL || ctx->handle == NULL) {
        return;
    }

    printf("\n[PCAP] ===== 캡처 통계 =====\n");
    printf("       캡처 패킷 : %llu\n",
           (unsigned long long)ctx->packets_captured);
    printf("       캡처 바이트: %llu\n",
           (unsigned long long)ctx->bytes_captured);

    if (pcap_stats(ctx->handle, &ps) == 0) {
        printf("       수신 패킷 : %u\n", ps.ps_recv);
        printf("       커널 드롭: %u\n",  ps.ps_drop);
        printf("       인터페이스 드롭: %u\n", ps.ps_ifdrop);
    }
    printf("[PCAP] ====================\n\n");
}

/* =========================================================================
 * 독립 실행 모드 (sentinel_pcap 바이너리)
 * ========================================================================= */

#ifdef PCAP_STANDALONE

/* 패킷 핸들러: 파싱 + 룰 평가 + 리포트 */
typedef struct {
    rule_engine_t *engine;
    reporter_t    *reporter;
} handler_ctx_t;

static void on_alert(const rule_alert_t *alert, void *userdata)
{
    reporter_t *rep = (reporter_t *)userdata;
    reporter_alert(rep, alert);

    printf("[ALERT] 규칙 #%u: %s\n", alert->rule_id, alert->msg);
}

static void packet_callback(const struct pcap_pkthdr *header,
                             const uint8_t *packet, void *userdata)
{
    handler_ctx_t *hctx = (handler_ctx_t *)userdata;
    parsed_packet_t parsed;

    if (parse_packet(packet, header->caplen, &parsed) != 0) {
        return;
    }

    parsed.timestamp = header->ts;
    parsed.raw_len   = header->caplen;

    print_parsed_packet(&parsed);

    reporter_update(hctx->reporter, &parsed);
    (void)rule_engine_evaluate(hctx->engine, &parsed,
                               on_alert, hctx->reporter);

    /* 10초마다 요약 */
    {
        static time_t last = 0;
        time_t now = time(NULL);
        if (now - last >= 10) {
            reporter_print_summary(hctx->reporter);
            last = now;
        }
    }
}

/**
 * @brief sentinel_pcap 진입점
 *
 * 사용법:
 *   sudo ./sentinel_pcap -i lo0
 *   sudo ./sentinel_pcap -i eth0
 *   ./sentinel_pcap -r capture.pcap --rules rules/default.rules --report out.csv
 */
int main(int argc, char *argv[])
{
    capture_ctx_t   cap;
    rule_engine_t   eng;
    reporter_t      rep;
    handler_ctx_t   hctx;
    char            iface[64]   = "lo0";
    char            infile[256] = {0};
    char            outcsv[256] = {0};
    bool            offline     = false;
    int             i;

    for (i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-i") == 0 && i+1 < argc) {
            (void)snprintf(iface, sizeof(iface), "%s", argv[++i]);
        }
        else if (strcmp(argv[i], "-r") == 0 && i+1 < argc) {
            (void)snprintf(infile, sizeof(infile), "%s", argv[++i]);
            offline = true;
        }
        else if (strcmp(argv[i], "--report") == 0 && i+1 < argc) {
            (void)snprintf(outcsv, sizeof(outcsv), "%s", argv[++i]);
        }
    }

    (void)rule_engine_init(&eng);
    (void)rule_engine_load_defaults(&eng);
    (void)reporter_init(&rep, outcsv[0] ? outcsv : NULL, false);

    hctx.engine   = &eng;
    hctx.reporter = &rep;

    if (offline) {
        (void)capture_open_offline(&cap, infile);
    } else {
        (void)capture_init(&cap, iface, "");
    }

    (void)capture_start(&cap, packet_callback, &hctx);

    reporter_print_summary(&rep);
    reporter_close(&rep);
    capture_close(&cap);
    rule_engine_print_stats(&eng);

    return 0;
}

#endif /* PCAP_STANDALONE */
