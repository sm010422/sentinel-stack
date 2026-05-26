/**
 * @file timer.h
 * @brief POSIX 고해상도 타이머 추상화 레이어
 *
 * clock_gettime(CLOCK_MONOTONIC) 기반의 타이머 유틸리티입니다.
 * Linux와 macOS 모두 동일한 인터페이스로 동작합니다.
 *
 * 사용 예:
 * @code
 *   timer_init();
 *   uint64_t t0 = timer_get_us();
 *   do_work();
 *   uint64_t elapsed = timer_get_us() - t0;
 * @endcode
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_TIMER_H
#define SENTINEL_TIMER_H

#include <stdint.h>
#include <time.h>

/* =========================================================================
 * 타이머 초기화
 * ========================================================================= */

/**
 * @brief 타이머 서브시스템 초기화
 *
 * 기준 시각(epoch)을 기록합니다. 이후 timer_get_ms() / timer_get_us()는
 * 이 시각으로부터의 경과 시간을 반환합니다.
 *
 * @return 0 성공, -1 실패
 */
int timer_init(void);

/* =========================================================================
 * 현재 시각 조회
 * ========================================================================= */

/**
 * @brief 현재 시각을 밀리초(ms)로 반환
 *
 * timer_init() 호출 이후의 경과 시간입니다.
 *
 * @return 경과 시간 (ms), 실패 시 0
 */
uint64_t timer_get_ms(void);

/**
 * @brief 현재 시각을 마이크로초(µs)로 반환
 *
 * @return 경과 시간 (µs), 실패 시 0
 */
uint64_t timer_get_us(void);

/**
 * @brief 현재 Unix 타임스탬프를 밀리초로 반환
 *
 * 패킷 타임스탬프 등 절대 시각이 필요한 경우에 사용합니다.
 *
 * @return Unix timestamp (ms)
 */
uint64_t timer_unix_ms(void);

/* =========================================================================
 * 슬립
 * ========================================================================= */

/**
 * @brief 지정한 마이크로초만큼 슬립
 *
 * nanosleep() 기반으로 시그널 인터럽트 시 재시도합니다.
 *
 * @param us 슬립 시간 (µs)
 */
void timer_sleep_us(uint32_t us);

/**
 * @brief 지정한 밀리초만큼 슬립
 *
 * @param ms 슬립 시간 (ms)
 */
void timer_sleep_ms(uint32_t ms);

/* =========================================================================
 * timespec 유틸리티
 * ========================================================================= */

/**
 * @brief struct timespec 를 마이크로초로 변환
 *
 * @param ts 변환할 timespec
 * @return 마이크로초 값
 */
uint64_t timespec_to_us(const struct timespec *ts);

/**
 * @brief 마이크로초를 struct timespec 으로 변환
 *
 * @param us    마이크로초 값
 * @param ts    결과를 저장할 timespec 포인터
 */
void us_to_timespec(uint64_t us, struct timespec *ts);

#endif /* SENTINEL_TIMER_H */
