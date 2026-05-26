/**
 * @file rms.c
 * @brief Rate Monotonic Scheduling (RMS) 구현
 *
 * Liu & Layland (1973) 논문 기반의 RMS 알고리즘입니다.
 *
 * 핵심 원리:
 *  - 주기가 짧을수록 높은 우선순위 부여 (정적 우선순위)
 *  - 스케줄 가능성 조건: U = Σ(WCET_i / T_i) ≤ n(2^(1/n) − 1)
 *  - n → ∞ 극한: U_bound → ln(2) ≈ 0.6931
 *
 * 방산 적용 맥락:
 *  MIL-STD-1553 버스 관리자는 1ms 주기의 고우선순위 태스크로,
 *  상태 모니터링은 100ms 주기의 저우선순위 태스크로 모델링됩니다.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "scheduler.h"
#include "timer.h"

#include <math.h>     /* pow(), log() */
#include <stdio.h>    /* printf */
#include <stdint.h>
#include <assert.h>

/* =========================================================================
 * 내부 함수
 * ========================================================================= */

/**
 * @brief 버블 정렬로 태스크를 period_ms 기준 오름차순 정렬
 *
 * 태스크 수가 MAX_TASKS(32) 이하로 제한되므로 O(n²)이어도 무방합니다.
 *
 * @param s 스케줄러 포인터
 */
static void sort_tasks_by_period(scheduler_t *s)
{
    uint32_t i;
    uint32_t j;
    rtos_task_t tmp;

    assert(s != NULL);

    for (i = 0U; i < s->task_count; i++) {
        for (j = i + 1U; j < s->task_count; j++) {
            if (s->tasks[j].period_ms < s->tasks[i].period_ms) {
                tmp           = s->tasks[i];
                s->tasks[i]   = s->tasks[j];
                s->tasks[j]   = tmp;
            }
        }
    }
}

/* =========================================================================
 * 공개 함수
 * ========================================================================= */

void rms_assign_priorities(scheduler_t *s)
{
    uint32_t i;

    assert(s != NULL);

    /* 주기 오름차순으로 정렬 */
    sort_tasks_by_period(s);

    /* 정렬 후 인덱스 순서대로 0(최고) ~ n-1(최저) 우선순위 부여 */
    for (i = 0U; i < s->task_count; i++) {
        s->tasks[i].priority = (uint8_t)i;
    }
}

int rms_select_next(const scheduler_t *s)
{
    uint32_t i;
    int      best_idx  = -1;
    uint8_t  best_prio = TASK_PRIORITY_MIN; /* 낮은 값이 높은 우선순위 */

    assert(s != NULL);

    for (i = 0U; i < s->task_count; i++) {
        if (s->tasks[i].state == TASK_READY) {
            if (s->tasks[i].priority <= best_prio) {
                best_prio = s->tasks[i].priority;
                best_idx  = (int)i;
            }
        }
    }

    return best_idx;
}

double rms_utilization(const scheduler_t *s)
{
    uint32_t i;
    double   util = 0.0;

    assert(s != NULL);

    for (i = 0U; i < s->task_count; i++) {
        if (s->tasks[i].period_ms > 0U) {
            /* WCET (µs) / period (µs) */
            double wcet_ms = (double)s->tasks[i].wcet_us / 1000.0;
            util += wcet_ms / (double)s->tasks[i].period_ms;
        }
    }

    return util;
}

int sched_validate_rms(const scheduler_t *s)
{
    double util;
    double bound;
    uint32_t n;

    if (s == NULL) {
        return -1;
    }

    n    = s->task_count;
    util = rms_utilization(s);

    if (n == 0U) {
        return 1; /* 태스크 없으면 스케줄 가능 */
    }

    /* Liu & Layland 한계: U_bound = n * (2^(1/n) - 1) */
    bound = (double)n * (pow(2.0, 1.0 / (double)n) - 1.0);

    printf("[RMS] CPU 사용률: %.4f (한계: %.4f, %s)\n",
           util, bound,
           (util <= bound) ? "스케줄 가능" : "한계 초과 — 데드라인 미스 가능성 있음");

    return (util <= bound) ? 1 : 0;
}
