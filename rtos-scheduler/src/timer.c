/**
 * @file timer.c
 * @brief POSIX 고해상도 타이머 구현
 *
 * clock_gettime(CLOCK_MONOTONIC) 기반 구현으로 Linux와 macOS 모두 지원합니다.
 * macOS에서는 동일한 POSIX 인터페이스를 사용하므로 조건부 컴파일이 불필요합니다.
 *
 * 정밀도: 나노초 해상도, 실제 지터는 OS 스케줄러 의존 (~1-10 µs)
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "timer.h"

#include <stddef.h>   /* NULL */
#include <errno.h>
#include <string.h>

/* =========================================================================
 * 모듈 내부 상태 (정적 변수)
 * ========================================================================= */

/** 타이머 기준 시각 (timer_init() 호출 시점) */
static struct timespec s_epoch = {0, 0};

/** 초기화 완료 여부 */
static int s_initialized = 0;

/* =========================================================================
 * 구현
 * ========================================================================= */

int timer_init(void)
{
    if (clock_gettime(CLOCK_MONOTONIC, &s_epoch) != 0) {
        return -1;
    }
    s_initialized = 1;
    return 0;
}

uint64_t timer_get_us(void)
{
    struct timespec now;

    if (s_initialized == 0) {
        return 0U;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0U;
    }

    /* 오버플로 방지: 시작 시각으로부터의 차이만 계산 */
    int64_t sec_diff  = (int64_t)now.tv_sec  - (int64_t)s_epoch.tv_sec;
    int64_t nsec_diff = (int64_t)now.tv_nsec - (int64_t)s_epoch.tv_nsec;

    int64_t total_us = sec_diff * 1000000LL + nsec_diff / 1000LL;

    return (total_us >= 0) ? (uint64_t)total_us : 0U;
}

uint64_t timer_get_ms(void)
{
    return timer_get_us() / 1000U;
}

uint64_t timer_unix_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return 0U;
    }

    return (uint64_t)now.tv_sec * 1000ULL + (uint64_t)(now.tv_nsec / 1000000LL);
}

void timer_sleep_us(uint32_t us)
{
    struct timespec req;
    struct timespec rem;

    req.tv_sec  = (time_t)(us / 1000000U);
    req.tv_nsec = (long)((us % 1000000U) * 1000UL);

    /* EINTR 인터럽트 발생 시 남은 시간으로 재시도 */
    while (nanosleep(&req, &rem) != 0) {
        if (errno != EINTR) {
            break;
        }
        req = rem;
    }
}

void timer_sleep_ms(uint32_t ms)
{
    timer_sleep_us(ms * 1000U);
}

uint64_t timespec_to_us(const struct timespec *ts)
{
    if (ts == NULL) {
        return 0U;
    }
    return (uint64_t)ts->tv_sec * 1000000ULL + (uint64_t)(ts->tv_nsec / 1000LL);
}

void us_to_timespec(uint64_t us, struct timespec *ts)
{
    if (ts == NULL) {
        return;
    }
    ts->tv_sec  = (time_t)(us / 1000000ULL);
    ts->tv_nsec = (long)((us % 1000000ULL) * 1000UL);
}
