/**
 * @file edf.c
 * @brief Earliest Deadline First (EDF) 스케줄러 구현
 *
 * EDF는 최적 단일 프로세서 동적 스케줄링 알고리즘입니다.
 * CPU 사용률 U ≤ 1.0 이면 반드시 스케줄 가능합니다 (RMS보다 높은 한계).
 *
 * 동작 방식:
 *  - 매 태스크 활성화 시 절대 데드라인 = 활성화 시각 + deadline_ms
 *  - READY 태스크 중 abs_deadline_ms 가 가장 작은 태스크를 선택
 *  - 새 태스크 활성화 시 선점 발생 (더 급한 데드라인이 있으면 교체)
 *
 * 방산 적용 맥락:
 *  비정기적으로 발생하는 긴급 명령(CMD) 처리에 EDF가 적합합니다.
 *  정기 센서 데이터와 비정기 명령이 혼재하는 환경에서 유연성을 제공합니다.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "scheduler.h"
#include "timer.h"

#include <stdint.h>
#include <assert.h>
#include <stdio.h>

/* =========================================================================
 * 공개 함수
 * ========================================================================= */

void edf_update_deadlines(scheduler_t *s, uint64_t current_ms)
{
    uint32_t i;

    assert(s != NULL);

    for (i = 0U; i < s->task_count; i++) {
        rtos_task_t *t = &s->tasks[i];

        /*
         * 태스크가 새로 활성화될 때(READY로 전환될 때)만 갱신합니다.
         * next_activation_ms 에 도달한 시점을 활성화 시각으로 간주합니다.
         */
        if ((t->state == TASK_READY) && (t->abs_deadline_ms == 0U)) {
            t->abs_deadline_ms = current_ms + (uint64_t)t->deadline_ms;
        }
    }
}

int edf_select_next(const scheduler_t *s)
{
    uint32_t i;
    int      best_idx      = -1;
    uint64_t earliest_dl   = UINT64_MAX;

    assert(s != NULL);

    for (i = 0U; i < s->task_count; i++) {
        const rtos_task_t *t = &s->tasks[i];

        if (t->state == TASK_READY) {
            if (t->abs_deadline_ms < earliest_dl) {
                earliest_dl = t->abs_deadline_ms;
                best_idx    = (int)i;
            }
        }
    }

    return best_idx;
}
