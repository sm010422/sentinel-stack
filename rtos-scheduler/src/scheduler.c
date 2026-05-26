/**
 * @file scheduler.c
 * @brief RTOS 스케줄러 핵심 엔진 및 독립 실행 진입점
 *
 * 이 파일은 두 가지 역할을 합니다:
 *  1. 라이브러리 모드: sched_init(), sched_add_task(), sched_run() 등 공개 API 구현
 *  2. 독립 실행 모드(RTOS_STANDALONE): main() 포함, CLI 파라미터 파싱
 *
 * 시뮬레이션 모델:
 *  실제 RTOS는 하드웨어 인터럽트로 선점을 구현하지만,
 *  POSIX 시뮬레이션에서는 tick 루프 내에서 임의 태스크 교체를 통해
 *  선점 동작을 모델링합니다.
 *  실제 지터 측정: 예상 활성화 시각과 timer_get_us() 측정값의 차이.
 *
 * 빌드:
 *  라이브러리: -DRTOS_STANDALONE 없이 컴파일
 *  실행 파일:  -DRTOS_STANDALONE 으로 컴파일
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "scheduler.h"
#include "timer.h"
#include "task.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <math.h>
#include <stdlib.h>   /* atoi, strtoul */

/* =========================================================================
 * 내부 전역 상태
 * ========================================================================= */

/** SIGINT 수신 시 루프 종료 플래그 */
static volatile int g_stop_requested = 0;

/* =========================================================================
 * 내부 함수 전방 선언
 * ========================================================================= */

static void handle_sigint(int signum);
static void activate_ready_tasks(scheduler_t *s, uint64_t current_ms);
static void run_task(scheduler_t *s, int task_idx, uint64_t current_ms);
static void check_deadline_miss(scheduler_t *s, int task_idx, uint64_t finish_ms);
static void update_jitter(rtos_task_t *t, int64_t jitter_us);

/* =========================================================================
 * task.h 공개 함수 구현 (이 파일에서 함께 구현)
 * ========================================================================= */

const char *task_state_str(task_state_t state)
{
    switch (state) {
        case TASK_READY:     return "READY";
        case TASK_RUNNING:   return "RUNNING";
        case TASK_BLOCKED:   return "BLOCKED";
        case TASK_SUSPENDED: return "SUSPENDED";
        case TASK_DEAD:      return "DEAD";
        default:             return "UNKNOWN";
    }
}

void task_init_defaults(rtos_task_t *task)
{
    if (task == NULL) {
        return;
    }
    (void)memset(task, 0, sizeof(rtos_task_t));
    task->state       = TASK_READY;
    task->min_jitter_us = INT64_MAX;
    task->max_jitter_us = INT64_MIN;
    task->priority    = TASK_PRIORITY_MIN;
}

int task_validate(const rtos_task_t *task)
{
    if (task == NULL) {
        return -1;
    }
    if (task->period_ms == 0U) {
        fprintf(stderr, "[TASK] 오류: period_ms 는 0이면 안 됩니다 (task_id=%u)\n",
                task->task_id);
        return -1;
    }
    if (task->deadline_ms > task->period_ms) {
        fprintf(stderr, "[TASK] 오류: deadline_ms(%u) > period_ms(%u) (task_id=%u)\n",
                task->deadline_ms, task->period_ms, task->task_id);
        return -1;
    }
    if ((uint64_t)task->wcet_us > (uint64_t)task->deadline_ms * 1000ULL) {
        fprintf(stderr, "[TASK] 경고: wcet_us(%u) > deadline_ms*1000(%llu) (task_id=%u)\n",
                task->wcet_us,
                (unsigned long long)(task->deadline_ms * 1000U),
                task->task_id);
        /* 경고만 출력, 실패는 아님 */
    }
    return 0;
}

/* =========================================================================
 * 스케줄러 API 구현
 * ========================================================================= */

int sched_init(scheduler_t *s, sched_algo_t algo)
{
    if (s == NULL) {
        return -1;
    }

    (void)memset(s, 0, sizeof(scheduler_t));
    s->algo    = algo;
    s->running = false;

    if (timer_init() != 0) {
        fprintf(stderr, "[SCHED] 타이머 초기화 실패\n");
        return -1;
    }

    /* 시그널 핸들러 등록 */
    (void)signal(SIGINT, handle_sigint);

    printf("[SCHED] 스케줄러 초기화 완료 (알고리즘: %s)\n",
           (algo == SCHED_ALGO_RMS) ? "RMS" : "EDF");

    return 0;
}

int sched_add_task(scheduler_t *s, const rtos_task_t *task)
{
    rtos_task_t *slot;

    if (s == NULL || task == NULL) {
        return -1;
    }
    if (s->task_count >= MAX_TASKS) {
        fprintf(stderr, "[SCHED] 태스크 풀이 가득 찼습니다 (최대 %u)\n", MAX_TASKS);
        return -1;
    }
    if (task_validate(task) != 0) {
        return -1;
    }

    /* 정적 풀에 복사 */
    slot  = &s->tasks[s->task_count];
    *slot = *task;

    /* 런타임 상태 초기화 */
    slot->state               = TASK_READY;
    slot->activation_count    = 0U;
    slot->deadline_miss_count = 0U;
    slot->last_jitter_us      = 0;
    slot->max_jitter_us       = INT64_MIN;
    slot->min_jitter_us       = INT64_MAX;
    slot->sum_jitter_us       = 0;
    slot->next_activation_ms  = 0U; /* 첫 번째 tick에서 즉시 활성화 */
    slot->abs_deadline_ms     = 0U;

    s->task_count++;

    /* RMS: 태스크 추가마다 우선순위 재배정 */
    if (s->algo == SCHED_ALGO_RMS) {
        rms_assign_priorities(s);
    }

    printf("[SCHED] 태스크 등록: id=%u name='%s' period=%ums deadline=%ums wcet=%uus prio=%u\n",
           task->task_id, task->name,
           task->period_ms, task->deadline_ms,
           task->wcet_us, slot->priority);

    return 0;
}

int sched_run(scheduler_t *s, uint32_t duration_ms)
{
    uint64_t start_ms;
    uint64_t current_ms;
    uint64_t end_ms;
    int      next_task;

    if (s == NULL || s->task_count == 0U) {
        return -1;
    }

    /* RMS 스케줄 가능성 사전 검사 */
    if (s->algo == SCHED_ALGO_RMS) {
        (void)sched_validate_rms(s);
    }

    s->running         = true;
    g_stop_requested   = 0;
    start_ms           = timer_get_ms();
    s->start_time_us   = timer_get_us();
    end_ms             = (duration_ms > 0U) ? (start_ms + (uint64_t)duration_ms) : UINT64_MAX;

    printf("[RTOS] 스케줄러 시작: 알고리즘=%s, 태스크 수=%u, 실행시간=%ums\n",
           (s->algo == SCHED_ALGO_RMS) ? "RMS" : "EDF",
           s->task_count,
           duration_ms);

    /* ── 메인 시뮬레이션 루프 ─────────────────────────────────────── */
    while ((s->running) && (g_stop_requested == 0)) {
        current_ms = timer_get_ms() - start_ms;

        if ((duration_ms > 0U) && (current_ms >= (uint64_t)duration_ms)) {
            break;
        }

        /* 1. 이번 tick에서 활성화할 태스크를 READY로 전환 */
        activate_ready_tasks(s, current_ms);

        /* EDF: 절대 데드라인 갱신 */
        if (s->algo == SCHED_ALGO_EDF) {
            edf_update_deadlines(s, current_ms);
        }

        /* 2. 다음 실행 태스크 선택 */
        if (s->algo == SCHED_ALGO_RMS) {
            next_task = rms_select_next(s);
        } else {
            next_task = edf_select_next(s);
        }

        /* 3. 태스크 실행 */
        if (next_task >= 0) {
            run_task(s, next_task, current_ms);
        } else {
            /* 유휴(Idle): 1ms 대기 */
            timer_sleep_ms(1U);
        }

        s->tick_ms = current_ms;
    }

    s->running = false;
    printf("[RTOS] 스케줄러 종료 (실행 시간: %llums)\n",
           (unsigned long long)(timer_get_ms() - start_ms));

    return 0;
}

void sched_stop(scheduler_t *s)
{
    if (s != NULL) {
        s->running = false;
    }
    g_stop_requested = 1;
}

void sched_print_report(const scheduler_t *s)
{
    uint32_t i;
    double   avg_jitter;

    if (s == NULL) {
        return;
    }

    printf("\n[RTOS] ===== Jitter & Performance Report =====\n");
    printf("       알고리즘         : %s\n",
           (s->algo == SCHED_ALGO_RMS) ? "RMS" : "EDF");
    printf("       총 컨텍스트 스위치: %llu\n",
           (unsigned long long)s->context_switch_count);
    printf("       총 데드라인 미스  : %llu / %llu 활성화\n",
           (unsigned long long)s->total_deadline_miss,
           (unsigned long long)s->total_activations);

    if (s->task_count > 0U && s->algo == SCHED_ALGO_RMS) {
        printf("       RMS 사용률       : %.4f\n", rms_utilization(s));
    }

    printf("\n       %-12s %8s %8s %8s %8s %8s\n",
           "태스크명", "활성화", "미스", "avg(µs)", "max(µs)", "min(µs)");
    printf("       %s\n",
           "──────────────────────────────────────────────────────");

    for (i = 0U; i < s->task_count; i++) {
        const rtos_task_t *t = &s->tasks[i];

        avg_jitter = (t->activation_count > 0U)
                     ? ((double)t->sum_jitter_us / (double)t->activation_count)
                     : 0.0;

        printf("       %-12s %8llu %8llu %8.2f %8lld %8lld\n",
               t->name,
               (unsigned long long)t->activation_count,
               (unsigned long long)t->deadline_miss_count,
               avg_jitter,
               (long long)((t->max_jitter_us == INT64_MIN) ? 0 : t->max_jitter_us),
               (long long)((t->min_jitter_us == INT64_MAX) ? 0 : t->min_jitter_us));
    }

    printf("[RTOS] =============================================\n\n");
}

/* =========================================================================
 * 동기화 객체 API
 * ========================================================================= */

int sched_mutex_create(scheduler_t *s, uint8_t ceiling_priority)
{
    rtos_mutex_t *m;

    if (s == NULL || s->mutex_count >= MAX_MUTEXES) {
        return -1;
    }

    m = &s->mutexes[s->mutex_count];
    m->mutex_id         = s->mutex_count + 1U;
    m->locked           = false;
    m->owner_task_id    = 0U;
    m->ceiling_priority = ceiling_priority;
    m->lock_count       = 0U;

    s->mutex_count++;
    return (int)m->mutex_id;
}

int sched_mutex_lock(scheduler_t *s, uint32_t mutex_id, uint32_t task_id)
{
    rtos_mutex_t *m;

    if (s == NULL || mutex_id == 0U || mutex_id > s->mutex_count) {
        return -1;
    }

    m = &s->mutexes[mutex_id - 1U];

    if (m->locked && (m->owner_task_id != task_id)) {
        return -1; /* 이미 다른 태스크가 점유 */
    }

    m->locked        = true;
    m->owner_task_id = task_id;
    m->lock_count++;
    return 0;
}

int sched_mutex_unlock(scheduler_t *s, uint32_t mutex_id, uint32_t task_id)
{
    rtos_mutex_t *m;

    if (s == NULL || mutex_id == 0U || mutex_id > s->mutex_count) {
        return -1;
    }

    m = &s->mutexes[mutex_id - 1U];

    if (!m->locked || (m->owner_task_id != task_id)) {
        return -1;
    }

    m->lock_count--;
    if (m->lock_count == 0U) {
        m->locked        = false;
        m->owner_task_id = 0U;
    }
    return 0;
}

int sched_sema_create(scheduler_t *s, int32_t init_count, int32_t max_count)
{
    rtos_semaphore_t *sema;

    if (s == NULL || s->sema_count >= MAX_SEMAPHORES || init_count < 0 || max_count < 1) {
        return -1;
    }

    sema = &s->semaphores[s->sema_count];
    sema->sema_id      = s->sema_count + 1U;
    sema->count        = init_count;
    sema->max_count    = max_count;
    sema->waiting_count = 0U;
    (void)memset(sema->waiting_tasks, 0, sizeof(sema->waiting_tasks));

    s->sema_count++;
    return (int)sema->sema_id;
}

int sched_sema_wait(scheduler_t *s, uint32_t sema_id)
{
    rtos_semaphore_t *sema;

    if (s == NULL || sema_id == 0U || sema_id > s->sema_count) {
        return -1;
    }

    sema = &s->semaphores[sema_id - 1U];

    if (sema->count <= 0) {
        return -1; /* 카운트 부족 (블로킹은 시뮬레이션에서 생략) */
    }

    sema->count--;
    return 0;
}

int sched_sema_signal(scheduler_t *s, uint32_t sema_id)
{
    rtos_semaphore_t *sema;

    if (s == NULL || sema_id == 0U || sema_id > s->sema_count) {
        return -1;
    }

    sema = &s->semaphores[sema_id - 1U];

    if (sema->count >= sema->max_count) {
        return -1; /* 오버플로 방지 */
    }

    sema->count++;
    return 0;
}

/* =========================================================================
 * 내부 함수 구현
 * ========================================================================= */

static void handle_sigint(int signum)
{
    (void)signum;
    g_stop_requested = 1;
    printf("\n[RTOS] SIGINT 수신 — 스케줄러 종료 중...\n");
}

static void activate_ready_tasks(scheduler_t *s, uint64_t current_ms)
{
    uint32_t i;

    for (i = 0U; i < s->task_count; i++) {
        rtos_task_t *t = &s->tasks[i];

        if ((t->state == TASK_DEAD || t->state == TASK_READY) &&
            (current_ms >= t->next_activation_ms)) {

            /* 지터 측정: 실제 활성화 시각 vs 예상 시각 */
            int64_t jitter_us = ((int64_t)timer_get_us()
                                 - (int64_t)s->start_time_us
                                 - (int64_t)(t->next_activation_ms * 1000U));

            update_jitter(t, jitter_us);

            t->state              = TASK_READY;
            t->abs_deadline_ms    = 0U; /* EDF에서 재설정 */
            t->activation_count++;
            s->total_activations++;

            printf("[%7llums] %s (prio=%u) ACTIVATED jitter=%+lldus\n",
                   (unsigned long long)current_ms,
                   t->name, t->priority,
                   (long long)jitter_us);
        }
    }
}

static void run_task(scheduler_t *s, int task_idx, uint64_t current_ms)
{
    rtos_task_t *t;
    uint64_t     actual_start_us;
    uint64_t     actual_finish_us;
    uint64_t     finish_ms;

    assert(task_idx >= 0 && (uint32_t)task_idx < s->task_count);

    t = &s->tasks[task_idx];
    t->state = TASK_RUNNING;

    printf("[%7llums] %s RUNNING (wcet=%uus)\n",
           (unsigned long long)current_ms,
           t->name, t->wcet_us);

    actual_start_us = timer_get_us();

    /* 태스크 함수 호출 (또는 WCET 시뮬레이션) */
    if (t->task_func != NULL) {
        t->task_func(t->arg);
    } else {
        /* WCET 시뮬레이션: 실제 슬립으로 CPU 사용 모방 */
        timer_sleep_us(t->wcet_us);
    }

    actual_finish_us = timer_get_us();
    finish_ms        = actual_finish_us / 1000U - s->start_time_us / 1000U;

    printf("[%7llums] %s DONE (실행시간=%lluus)\n",
           (unsigned long long)finish_ms,
           t->name,
           (unsigned long long)(actual_finish_us - actual_start_us));

    /* 데드라인 체크 */
    check_deadline_miss(s, task_idx, finish_ms);

    /* 다음 주기 설정 */
    t->next_activation_ms += (uint64_t)t->period_ms;
    t->abs_deadline_ms     = 0U;
    t->state               = TASK_DEAD; /* 다음 주기 활성화 전까지 DEAD */

    s->context_switch_count++;
}

static void check_deadline_miss(scheduler_t *s, int task_idx, uint64_t finish_ms)
{
    const rtos_task_t *t;

    assert(task_idx >= 0 && (uint32_t)task_idx < s->task_count);
    t = &s->tasks[task_idx];

    /* 절대 데드라인 = 마지막 활성화 시각 + deadline_ms */
    uint64_t abs_dl = t->next_activation_ms + (uint64_t)t->deadline_ms;

    if (finish_ms > abs_dl) {
        s->tasks[task_idx].deadline_miss_count++;
        s->total_deadline_miss++;
        fprintf(stderr, "[RTOS] !! 데드라인 미스: %s (완료=%llums, 마감=%llums)\n",
                t->name,
                (unsigned long long)finish_ms,
                (unsigned long long)abs_dl);
    }
}

static void update_jitter(rtos_task_t *t, int64_t jitter_us)
{
    t->last_jitter_us = jitter_us;
    t->sum_jitter_us += jitter_us;

    if (jitter_us > t->max_jitter_us) {
        t->max_jitter_us = jitter_us;
    }
    if (jitter_us < t->min_jitter_us) {
        t->min_jitter_us = jitter_us;
    }
}

/* =========================================================================
 * 독립 실행 모드 (rtos_sim 바이너리)
 * ========================================================================= */

#ifdef RTOS_STANDALONE

/** 데모용 태스크 함수 — 실제 작업 없이 로그만 출력 */
static void demo_task(void *arg)
{
    (void)arg; /* 미사용 파라미터 경고 억제 */
}

/**
 * @brief rtos_sim 진입점
 *
 * 사용법:
 *   ./rtos_sim --algo rms|edf --tasks N --duration Xs
 *
 * 예:
 *   ./rtos_sim --algo rms --tasks 5 --duration 5s
 */
int main(int argc, char *argv[])
{
    scheduler_t  sched;
    sched_algo_t algo     = SCHED_ALGO_RMS;
    uint32_t     num_tasks = 3U;
    uint32_t     duration_ms = 5000U;
    int          i;

    /* ── CLI 파라미터 파싱 ────────────────────────────────── */
    for (i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--algo") == 0) {
            if (strcmp(argv[i + 1], "edf") == 0) {
                algo = SCHED_ALGO_EDF;
            } else {
                algo = SCHED_ALGO_RMS;
            }
        } else if (strcmp(argv[i], "--tasks") == 0) {
            num_tasks = (uint32_t)atoi(argv[i + 1]);
            if (num_tasks == 0U || num_tasks > MAX_TASKS) {
                num_tasks = 3U;
            }
        } else if (strcmp(argv[i], "--duration") == 0) {
            /* "10s" → 10000ms */
            duration_ms = (uint32_t)strtoul(argv[i + 1], NULL, 10) * 1000U;
        }
    }

    /* ── 스케줄러 초기화 ───────────────────────────────────── */
    if (sched_init(&sched, algo) != 0) {
        fprintf(stderr, "스케줄러 초기화 실패\n");
        return 1;
    }

    /* ── 데모 태스크 생성 ──────────────────────────────────── */
    /* MIL-STD-1553 버스 관리 태스크 모델: 1ms / 10ms / 100ms 주기 */
    static const uint32_t periods[]   = {  2,  5,  10,  20,  50, 100, 200, 500 };
    static const uint32_t wcets[]     = {300, 800, 1500, 3000, 8000, 15000, 30000, 80000 };
    static const char    *names[]     = {
        "BUS_MGR", "SENSOR", "NAV", "CTRL", "STATUS", "LOG", "DIAG", "HEALTH"
    };

    for (i = 0; i < (int)num_tasks && i < 8; i++) {
        rtos_task_t t;
        task_init_defaults(&t);
        t.task_id    = (uint32_t)(i + 1);
        t.period_ms  = periods[i];
        t.deadline_ms = periods[i]; /* Implicit deadline */
        t.wcet_us    = wcets[i];
        t.task_func  = demo_task;
        (void)snprintf(t.name, sizeof(t.name), "%s", names[i]);

        if (sched_add_task(&sched, &t) != 0) {
            fprintf(stderr, "태스크 등록 실패: %s\n", t.name);
        }
    }

    /* ── 스케줄러 실행 ─────────────────────────────────────── */
    (void)sched_run(&sched, duration_ms);

    /* ── 결과 리포트 ────────────────────────────────────────── */
    sched_print_report(&sched);

    return 0;
}

#endif /* RTOS_STANDALONE */
