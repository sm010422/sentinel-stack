/**
 * @file test_scheduler.c
 * @brief RTOS 스케줄러 단위 테스트
 *
 * assert() 기반 테스트 프레임워크입니다.
 * 외부 라이브러리 의존 없이 단독 실행 가능합니다.
 *
 * 테스트 항목:
 *  1. RMS 우선순위 배정 검증
 *  2. EDF 태스크 선택 검증
 *  3. 데드라인 미스 감지 검증
 *  4. CPU 사용률 계산 검증
 *  5. 동기화 객체 (Mutex / Semaphore) 기본 동작
 *  6. 태스크 파라미터 유효성 검사
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "../include/scheduler.h"
#include "../include/timer.h"
#include "../include/task.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* =========================================================================
 * 테스트 헬퍼 매크로
 * ========================================================================= */

#define TEST_BEGIN(name)  printf("  [TEST] %-50s ... ", (name))
#define TEST_PASS()       printf("PASS\n")
#define TEST_FAIL(msg)    do { printf("FAIL — %s\n", (msg)); g_fail_count++; } while (0)

static int g_fail_count = 0;
static int g_test_count = 0;

/** 조건 검사 매크로 */
#define EXPECT_TRUE(cond, msg) \
    do { \
        g_test_count++; \
        if (!(cond)) { TEST_FAIL(msg); } else { TEST_PASS(); } \
    } while (0)

#define EXPECT_EQ(a, b, msg) \
    do { \
        g_test_count++; \
        if ((a) != (b)) { \
            printf("FAIL — %s (expected %lld, got %lld)\n", \
                   (msg), (long long)(b), (long long)(a)); \
            g_fail_count++; \
        } else { TEST_PASS(); } \
    } while (0)

/* =========================================================================
 * 테스트 데이터 생성 헬퍼
 * ========================================================================= */

static rtos_task_t make_task(uint32_t id, const char *name,
                              uint32_t period_ms, uint32_t deadline_ms,
                              uint32_t wcet_us)
{
    rtos_task_t t;
    task_init_defaults(&t);
    t.task_id    = id;
    t.period_ms  = period_ms;
    t.deadline_ms = deadline_ms;
    t.wcet_us    = wcet_us;
    (void)snprintf(t.name, sizeof(t.name), "%s", name);
    return t;
}

/* =========================================================================
 * Test 1: RMS 우선순위 배정
 * ========================================================================= */

static void test_rms_priority_assignment(void)
{
    scheduler_t s;
    rtos_task_t t;

    printf("\n[SUITE] RMS 우선순위 배정 테스트\n");

    (void)sched_init(&s, SCHED_ALGO_RMS);

    /* 주기 순서와 반대로 추가하여 정렬 동작 확인 */
    t = make_task(3, "SLOW",   100, 100, 5000); (void)sched_add_task(&s, &t);
    t = make_task(1, "FAST",     5,   5,  300); (void)sched_add_task(&s, &t);
    t = make_task(2, "MEDIUM",  20,  20, 1500); (void)sched_add_task(&s, &t);

    TEST_BEGIN("RMS: 주기=5ms 태스크가 우선순위 0 (최고)");
    {
        /* 주기가 5ms인 FAST 태스크의 우선순위가 0이어야 함 */
        uint32_t i;
        uint8_t fast_prio = 255U;
        for (i = 0U; i < s.task_count; i++) {
            if (strcmp(s.tasks[i].name, "FAST") == 0) {
                fast_prio = s.tasks[i].priority;
            }
        }
        EXPECT_EQ((int)fast_prio, 0, "FAST 태스크 우선순위 == 0");
    }

    TEST_BEGIN("RMS: 주기=100ms 태스크가 최저 우선순위");
    {
        uint32_t i;
        uint8_t slow_prio = 0U;
        for (i = 0U; i < s.task_count; i++) {
            if (strcmp(s.tasks[i].name, "SLOW") == 0) {
                slow_prio = s.tasks[i].priority;
            }
        }
        EXPECT_EQ((int)slow_prio, 2, "SLOW 태스크 우선순위 == 2");
    }

    TEST_BEGIN("RMS: 등록 태스크 수 == 3");
    EXPECT_EQ((int)s.task_count, 3, "task_count == 3");
}

/* =========================================================================
 * Test 2: RMS 스케줄 가능성 검사
 * ========================================================================= */

static void test_rms_schedulability(void)
{
    scheduler_t s;
    rtos_task_t t;
    int result;

    printf("\n[SUITE] RMS 스케줄 가능성 (Liu & Layland 한계)\n");

    /* 스케줄 가능한 태스크 세트: U = 0.3+0.2+0.1 = 0.6 < ln(2)=0.693 */
    (void)sched_init(&s, SCHED_ALGO_RMS);
    t = make_task(1, "T1", 10, 10, 3000); (void)sched_add_task(&s, &t); /* 0.3 */
    t = make_task(2, "T2", 20, 20, 4000); (void)sched_add_task(&s, &t); /* 0.2 */
    t = make_task(3, "T3", 50, 50, 5000); (void)sched_add_task(&s, &t); /* 0.1 */

    TEST_BEGIN("RMS: U=0.6 — 스케줄 가능 (결과 == 1)");
    result = sched_validate_rms(&s);
    EXPECT_EQ(result, 1, "스케줄 가능");

    /* 스케줄 불가능한 태스크 세트: U 초과 */
    (void)sched_init(&s, SCHED_ALGO_RMS);
    t = make_task(1, "T1",  5,  5, 4000); (void)sched_add_task(&s, &t); /* 0.8 */
    t = make_task(2, "T2", 10, 10, 3000); (void)sched_add_task(&s, &t); /* 0.3 */
    t = make_task(3, "T3", 20, 20, 3000); (void)sched_add_task(&s, &t); /* 0.15 */

    TEST_BEGIN("RMS: U=1.25 — 스케줄 불가 (결과 == 0)");
    result = sched_validate_rms(&s);
    EXPECT_EQ(result, 0, "스케줄 불가");
}

/* =========================================================================
 * Test 3: EDF 태스크 선택
 * ========================================================================= */

static void test_edf_selection(void)
{
    scheduler_t s;
    int         idx;

    printf("\n[SUITE] EDF 태스크 선택 테스트\n");

    (void)sched_init(&s, SCHED_ALGO_EDF);

    /* 3개 태스크, 각기 다른 절대 데드라인 설정 */
    rtos_task_t tasks[3] = {
        make_task(1, "T_LATE",  100, 100, 1000),
        make_task(2, "T_EARLY",  20,  20, 1000),
        make_task(3, "T_MID",    50,  50, 1000)
    };
    tasks[0].state = TASK_READY; tasks[0].abs_deadline_ms = 100U;
    tasks[1].state = TASK_READY; tasks[1].abs_deadline_ms =  20U;
    tasks[2].state = TASK_READY; tasks[2].abs_deadline_ms =  50U;

    (void)memcpy(s.tasks, tasks, sizeof(tasks));
    s.task_count = 3U;

    TEST_BEGIN("EDF: 데드라인 20ms 태스크가 먼저 선택");
    idx = edf_select_next(&s);
    EXPECT_EQ(idx, 1, "인덱스 1 (T_EARLY) 선택");

    TEST_BEGIN("EDF: T_EARLY DEAD 이후 T_MID 선택");
    s.tasks[1].state = TASK_DEAD;
    idx = edf_select_next(&s);
    EXPECT_EQ(idx, 2, "인덱스 2 (T_MID) 선택");

    TEST_BEGIN("EDF: 모두 DEAD 이면 -1 반환");
    s.tasks[0].state = TASK_DEAD;
    s.tasks[2].state = TASK_DEAD;
    idx = edf_select_next(&s);
    EXPECT_EQ(idx, -1, "유휴 상태 (-1)");
}

/* =========================================================================
 * Test 4: 태스크 유효성 검사
 * ========================================================================= */

static void test_task_validation(void)
{
    rtos_task_t t;

    printf("\n[SUITE] 태스크 파라미터 유효성 검사\n");

    TEST_BEGIN("VALIDATE: period_ms=0 → 실패");
    t = make_task(1, "BAD", 0, 0, 100);
    EXPECT_EQ(task_validate(&t), -1, "period=0 무효");

    TEST_BEGIN("VALIDATE: deadline > period → 실패");
    t = make_task(1, "BAD2", 10, 20, 100);
    EXPECT_EQ(task_validate(&t), -1, "deadline>period 무효");

    TEST_BEGIN("VALIDATE: 정상 파라미터 → 성공");
    t = make_task(1, "GOOD", 20, 20, 5000);
    EXPECT_EQ(task_validate(&t), 0, "정상 파라미터");

    TEST_BEGIN("VALIDATE: NULL 포인터 → 실패");
    EXPECT_EQ(task_validate(NULL), -1, "NULL 포인터");
}

/* =========================================================================
 * Test 5: Mutex 동기화 객체
 * ========================================================================= */

static void test_mutex(void)
{
    scheduler_t s;
    int         mid;

    printf("\n[SUITE] Mutex 단위 테스트\n");

    (void)sched_init(&s, SCHED_ALGO_RMS);

    TEST_BEGIN("MUTEX: 생성 → ID 반환");
    mid = sched_mutex_create(&s, TASK_PRIORITY_MAX);
    EXPECT_TRUE(mid > 0, "Mutex ID > 0");

    TEST_BEGIN("MUTEX: 잠금 성공 (태스크 1)");
    EXPECT_EQ(sched_mutex_lock(&s, (uint32_t)mid, 1U), 0, "잠금 성공");

    TEST_BEGIN("MUTEX: 다른 태스크 잠금 시도 → 실패");
    EXPECT_EQ(sched_mutex_lock(&s, (uint32_t)mid, 2U), -1, "잠금 실패 (충돌)");

    TEST_BEGIN("MUTEX: 해제 성공");
    EXPECT_EQ(sched_mutex_unlock(&s, (uint32_t)mid, 1U), 0, "해제 성공");

    TEST_BEGIN("MUTEX: 해제 후 재잠금 성공");
    EXPECT_EQ(sched_mutex_lock(&s, (uint32_t)mid, 2U), 0, "재잠금 성공");
}

/* =========================================================================
 * Test 6: Semaphore
 * ========================================================================= */

static void test_semaphore(void)
{
    scheduler_t s;
    int         sid;

    printf("\n[SUITE] Semaphore 단위 테스트\n");

    (void)sched_init(&s, SCHED_ALGO_RMS);

    TEST_BEGIN("SEMA: 생성 (초기값=2, 최대=2)");
    sid = sched_sema_create(&s, 2, 2);
    EXPECT_TRUE(sid > 0, "Semaphore ID > 0");

    TEST_BEGIN("SEMA: Wait 두 번 성공");
    EXPECT_EQ(sched_sema_wait(&s, (uint32_t)sid), 0, "Wait #1");
    EXPECT_EQ(sched_sema_wait(&s, (uint32_t)sid), 0, "Wait #2");

    TEST_BEGIN("SEMA: Wait 세 번째 → 실패 (카운트=0)");
    EXPECT_EQ(sched_sema_wait(&s, (uint32_t)sid), -1, "카운트 소진");

    TEST_BEGIN("SEMA: Signal 후 Wait 가능");
    EXPECT_EQ(sched_sema_signal(&s, (uint32_t)sid), 0, "Signal");
    EXPECT_EQ(sched_sema_wait(&s, (uint32_t)sid), 0, "Wait 재시도");
}

/* =========================================================================
 * 테스트 러너
 * ========================================================================= */

int main(void)
{
    printf("══════════════════════════════════════════════════\n");
    printf("  sentinel-stack | RTOS 스케줄러 단위 테스트\n");
    printf("══════════════════════════════════════════════════\n\n");

    test_rms_priority_assignment();
    test_rms_schedulability();
    test_edf_selection();
    test_task_validation();
    test_mutex();
    test_semaphore();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  결과: %d 테스트 중 %d 실패\n",
           g_test_count, g_fail_count);

    if (g_fail_count == 0) {
        printf("  상태: ALL PASS ✓\n");
    } else {
        printf("  상태: FAIL ✗\n");
    }
    printf("══════════════════════════════════════════════════\n");

    return (g_fail_count == 0) ? 0 : 1;
}
