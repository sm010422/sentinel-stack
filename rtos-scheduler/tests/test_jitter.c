/**
 * @file test_jitter.c
 * @brief RTOS 지터(Jitter) 측정 벤치마크
 *
 * 실제 POSIX 타이머를 사용하여 태스크 활성화 지터를 측정합니다.
 * 방산 실시간 시스템의 타이밍 검증에서 사용하는 동일한 측정 방법론을 적용합니다.
 *
 * 측정 방법:
 *  예상 활성화 시각 = n * period_ms (정수 ms 단위)
 *  실제 활성화 시각 = timer_get_us() 측정값
 *  지터 = 실제 - 예상 (양수 = 지연, 음수 = 조기)
 *
 * 검증 기준 (macOS M2 기준):
 *  - 평균 지터 < 5 µs
 *  - 최대 지터 < 50 µs
 *  - 데드라인 미스율 0%
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "../include/scheduler.h"
#include "../include/timer.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* =========================================================================
 * 테스트 파라미터
 * ========================================================================= */

/** 지터 측정 실행 시간 (ms) */
#define BENCH_DURATION_MS    3000U

/** 최대 허용 지터 기준값 (µs) — 이를 초과하면 WARN */
#define JITTER_WARN_US       50LL

/** 평균 지터 경고 기준 (µs) */
#define JITTER_AVG_WARN_US   10.0

/* =========================================================================
 * 고해상도 지터 직접 측정
 * ========================================================================= */

/**
 * @brief 타이머 해상도 측정
 *
 * clock_gettime() 연속 호출 간격을 1000번 측정하여
 * 시스템 타이머 해상도를 파악합니다.
 */
static void measure_timer_resolution(void)
{
    struct timespec t1, t2;
    int64_t min_delta_ns = INT64_MAX;
    int64_t max_delta_ns = 0;
    int64_t sum_ns = 0;
    int i;
    const int N = 1000;

    for (i = 0; i < N; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t1);
        clock_gettime(CLOCK_MONOTONIC, &t2);

        int64_t delta = ((int64_t)t2.tv_sec - (int64_t)t1.tv_sec) * 1000000000LL
                        + ((int64_t)t2.tv_nsec - (int64_t)t1.tv_nsec);

        if (delta > 0) {
            if (delta < min_delta_ns) min_delta_ns = delta;
            if (delta > max_delta_ns) max_delta_ns = delta;
            sum_ns += delta;
        }
    }

    printf("[TIMER] 해상도 측정 (n=%d):\n", N);
    printf("         최소 간격 : %lld ns\n", (long long)min_delta_ns);
    printf("         최대 간격 : %lld ns\n", (long long)max_delta_ns);
    printf("         평균 간격 : %.1f ns\n", (double)sum_ns / N);
    printf("\n");
}

/* =========================================================================
 * 직접 지터 측정 (스케줄러 외부)
 * ========================================================================= */

#define DIRECT_MEAS_PERIOD_US  1000U  /* 1ms 주기 */
#define DIRECT_MEAS_COUNT      500U   /* 500 샘플 */

/**
 * @brief 1ms 주기 nanosleep 지터를 직접 측정
 *
 * 스케줄러 없이 nanosleep() 자체의 지터를 측정합니다.
 * 이것이 스케줄러 지터의 하한선(floor)입니다.
 */
static void measure_sleep_jitter(void)
{
    uint32_t i;
    int64_t  jitters[DIRECT_MEAS_COUNT];
    int64_t  sum = 0;
    int64_t  max_j = INT64_MIN;
    int64_t  min_j = INT64_MAX;
    int64_t  expected_us;
    int64_t  actual_us;
    uint64_t start_us;
    int      exceed_count = 0;

    printf("[JITTER] nanosleep(1ms) 지터 직접 측정 (%u 샘플)\n", DIRECT_MEAS_COUNT);

    timer_init();
    start_us = (int64_t)timer_get_us();

    for (i = 0U; i < DIRECT_MEAS_COUNT; i++) {
        expected_us = (int64_t)((i + 1U) * DIRECT_MEAS_PERIOD_US);
        timer_sleep_us(DIRECT_MEAS_PERIOD_US);
        actual_us = (int64_t)timer_get_us() - start_us;

        jitters[i] = actual_us - expected_us;
        sum += jitters[i];

        if (jitters[i] > max_j) max_j = jitters[i];
        if (jitters[i] < min_j) min_j = jitters[i];

        if (jitters[i] > JITTER_WARN_US) {
            exceed_count++;
        }
    }

    double avg = (double)sum / DIRECT_MEAS_COUNT;

    printf("         평균 지터 : %+.2f µs\n", avg);
    printf("         최대 지터 : %+lld µs\n", (long long)max_j);
    printf("         최소 지터 : %+lld µs\n", (long long)min_j);
    printf("         >%lldµs 횟수: %d / %u (%.1f%%)\n",
           (long long)JITTER_WARN_US, exceed_count, DIRECT_MEAS_COUNT,
           (double)exceed_count / DIRECT_MEAS_COUNT * 100.0);

    /* CSV 형식 일부 출력 */
    printf("\n[JITTER] 처음 10 샘플 (µs):\n         ");
    for (i = 0U; i < 10U && i < DIRECT_MEAS_COUNT; i++) {
        printf("%+lld ", (long long)jitters[i]);
    }
    printf("\n\n");

    /* 검증 */
    if (avg > JITTER_AVG_WARN_US) {
        printf("[WARN] 평균 지터가 목표치(%g µs)를 초과합니다.\n",
               JITTER_AVG_WARN_US);
    }
    if (max_j > JITTER_WARN_US) {
        printf("[WARN] 최대 지터가 목표치(%lld µs)를 초과합니다.\n",
               (long long)JITTER_WARN_US);
    }
}

/* =========================================================================
 * 스케줄러 통합 지터 측정
 * ========================================================================= */

static void task_noop(void *arg) { (void)arg; }

/**
 * @brief RMS / EDF 스케줄러로 3개 태스크를 실행하며 지터 측정
 */
static void measure_scheduler_jitter(sched_algo_t algo, const char *algo_name)
{
    scheduler_t  s;
    rtos_task_t  t;

    printf("[JITTER] %s 스케줄러 지터 측정 (실행시간=%dms)\n",
           algo_name, BENCH_DURATION_MS);

    (void)sched_init(&s, algo);

    /* 3개 태스크: 버스관리(2ms), 센서(10ms), 상태(50ms) */
    t.task_id = 1; (void)snprintf(t.name, sizeof(t.name), "BUS_MGR");
    t.period_ms = 2U; t.deadline_ms = 2U; t.wcet_us = 100U;
    t.task_func = task_noop; t.arg = NULL;
    t.state = TASK_READY; t.next_activation_ms = 0U; t.abs_deadline_ms = 0U;
    t.activation_count = 0U; t.deadline_miss_count = 0U;
    t.max_jitter_us = INT64_MIN; t.min_jitter_us = INT64_MAX;
    t.sum_jitter_us = 0; t.last_jitter_us = 0;
    t.priority = TASK_PRIORITY_MIN;
    (void)sched_add_task(&s, &t);

    t.task_id = 2; (void)snprintf(t.name, sizeof(t.name), "SENSOR");
    t.period_ms = 10U; t.deadline_ms = 10U; t.wcet_us = 500U;
    t.task_func = task_noop; t.arg = NULL;
    t.state = TASK_READY; t.next_activation_ms = 0U; t.abs_deadline_ms = 0U;
    t.activation_count = 0U; t.deadline_miss_count = 0U;
    t.max_jitter_us = INT64_MIN; t.min_jitter_us = INT64_MAX;
    t.sum_jitter_us = 0; t.last_jitter_us = 0;
    t.priority = TASK_PRIORITY_MIN;
    (void)sched_add_task(&s, &t);

    t.task_id = 3; (void)snprintf(t.name, sizeof(t.name), "STATUS");
    t.period_ms = 50U; t.deadline_ms = 50U; t.wcet_us = 2000U;
    t.task_func = task_noop; t.arg = NULL;
    t.state = TASK_READY; t.next_activation_ms = 0U; t.abs_deadline_ms = 0U;
    t.activation_count = 0U; t.deadline_miss_count = 0U;
    t.max_jitter_us = INT64_MIN; t.min_jitter_us = INT64_MAX;
    t.sum_jitter_us = 0; t.last_jitter_us = 0;
    t.priority = TASK_PRIORITY_MIN;
    (void)sched_add_task(&s, &t);

    (void)sched_run(&s, BENCH_DURATION_MS);
    sched_print_report(&s);

    /* 검증 */
    {
        uint32_t i;
        int all_pass = 1;
        for (i = 0U; i < s.task_count; i++) {
            const rtos_task_t *task = &s.tasks[i];
            if (task->deadline_miss_count > 0U) {
                printf("[FAIL] %s: 데드라인 미스 %llu회\n",
                       task->name,
                       (unsigned long long)task->deadline_miss_count);
                all_pass = 0;
            }
            if (task->max_jitter_us > JITTER_WARN_US) {
                printf("[WARN] %s: 최대 지터 %lld µs > 목표 %lld µs\n",
                       task->name,
                       (long long)task->max_jitter_us,
                       (long long)JITTER_WARN_US);
            }
        }
        if (all_pass) {
            printf("[PASS] %s: 데드라인 미스 없음\n", algo_name);
        }
    }
}

/* =========================================================================
 * 진입점
 * ========================================================================= */

int main(void)
{
    printf("══════════════════════════════════════════════════\n");
    printf("  sentinel-stack | RTOS Jitter Benchmark\n");
    printf("══════════════════════════════════════════════════\n\n");

    measure_timer_resolution();
    measure_sleep_jitter();

    printf("──────────────────────────────────────────────────\n\n");
    measure_scheduler_jitter(SCHED_ALGO_RMS, "RMS");

    printf("──────────────────────────────────────────────────\n\n");
    measure_scheduler_jitter(SCHED_ALGO_EDF, "EDF");

    printf("══════════════════════════════════════════════════\n");
    printf("  벤치마크 완료\n");
    printf("══════════════════════════════════════════════════\n");

    return 0;
}
