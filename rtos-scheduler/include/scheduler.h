/**
 * @file scheduler.h
 * @brief RTOS 스케줄러 엔진 공개 API
 *
 * POSIX 환경에서 선점형 실시간 스케줄러를 시뮬레이션합니다.
 * RMS(Rate Monotonic Scheduling)와 EDF(Earliest Deadline First) 알고리즘을
 * 모두 지원하며, 단일 인터페이스로 전환 가능합니다.
 *
 * 아키텍처 개요:
 * @code
 *   scheduler_t ─┬─ [RMS] 짧은 주기 = 높은 우선순위 (정적)
 *                └─ [EDF] 가장 가까운 데드라인 = 최고 우선순위 (동적)
 *
 *   sched_run() 루프:
 *     1. tick 증가
 *     2. 활성화 시각 도달한 태스크 READY 상태로 전환
 *     3. 알고리즘 별 다음 태스크 선택
 *     4. task_func() 호출 (또는 WCET 시뮬레이션)
 *     5. 실행 완료 후 지터 측정, 데드라인 체크
 * @endcode
 *
 * 참고 표준:
 *  - ARINC 653: 파티셔닝 스케줄러 개념 참고
 *  - Liu & Layland (1973): RMS 이론적 한계 U ≤ n(2^(1/n)−1)
 *
 * @author  Park Sang Min
 * @date    2025
 * @version 1.0
 */

#ifndef SENTINEL_SCHEDULER_H
#define SENTINEL_SCHEDULER_H

#include "task.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * 스케줄링 알고리즘
 * ========================================================================= */

/**
 * @brief 지원하는 스케줄링 알고리즘
 */
typedef enum {
    SCHED_ALGO_RMS = 0, /**< Rate Monotonic Scheduling — 정적 우선순위 */
    SCHED_ALGO_EDF = 1  /**< Earliest Deadline First   — 동적 우선순위 */
} sched_algo_t;

/* =========================================================================
 * 스케줄러 제어 블록
 * ========================================================================= */

/**
 * @brief 스케줄러 전역 상태 구조체
 *
 * 모든 태스크, 동기화 객체, 통계를 하나의 구조체에 보유합니다.
 * 단일 인스턴스를 정적으로 선언하여 사용합니다.
 */
typedef struct {
    /* ── 태스크 풀 ─────────────────────────────────────── */
    rtos_task_t  tasks[MAX_TASKS];   /**< 정적 TCB 배열 */
    uint32_t     task_count;         /**< 등록된 태스크 수 */

    /* ── 알고리즘 설정 ──────────────────────────────────── */
    sched_algo_t algo;               /**< 현재 적용 중인 알고리즘 */

    /* ── 런타임 상태 ─────────────────────────────────────── */
    bool         running;            /**< 스케줄러 실행 중 여부 */
    uint64_t     tick_ms;            /**< 현재 시뮬레이션 시각 (ms) */
    uint64_t     start_time_us;      /**< 실제 시작 시각 (µs, 지터 측정용) */

    /* ── 통계 ────────────────────────────────────────────── */
    uint64_t     context_switch_count; /**< 총 컨텍스트 스위치 횟수 */
    uint64_t     total_deadline_miss;  /**< 전체 데드라인 미스 횟수 */
    uint64_t     total_activations;    /**< 전체 태스크 활성화 횟수 */

    /* ── 동기화 객체 풀 ─────────────────────────────────── */
    rtos_mutex_t     mutexes[MAX_MUTEXES];       /**< Mutex 정적 풀 */
    uint32_t         mutex_count;

    rtos_semaphore_t semaphores[MAX_SEMAPHORES]; /**< Semaphore 정적 풀 */
    uint32_t         sema_count;

    rtos_msgqueue_t  msgqueues[MAX_MUTEXES];     /**< Message Queue 풀 */
    uint32_t         queue_count;
} scheduler_t;

/* =========================================================================
 * 스케줄러 API
 * ========================================================================= */

/**
 * @brief 스케줄러 초기화
 *
 * scheduler_t 구조체를 초기화하고 타이머 서브시스템을 시작합니다.
 *
 * @param s     스케줄러 포인터
 * @param algo  사용할 스케줄링 알고리즘
 * @return 0 성공, -1 실패
 */
int sched_init(scheduler_t *s, sched_algo_t algo);

/**
 * @brief 태스크 등록
 *
 * 스케줄러에 태스크를 추가합니다.
 * RMS 사용 시 이 함수가 자동으로 우선순위를 재배정합니다.
 *
 * 유효성 검사:
 *  - task_count < MAX_TASKS
 *  - wcet_us <= deadline_ms * 1000
 *  - deadline_ms <= period_ms
 *
 * @param s     스케줄러 포인터
 * @param task  등록할 태스크 (복사됨)
 * @return 0 성공, -1 실패 (포화 또는 유효성 오류)
 */
int sched_add_task(scheduler_t *s, const rtos_task_t *task);

/**
 * @brief 스케줄러 실행
 *
 * 지정한 시간(duration_ms) 동안 스케줄링 루프를 실행합니다.
 * 실행 중 SIGINT를 받으면 조기 종료합니다.
 *
 * 내부 동작:
 *  1. 시뮬레이션 tick을 1ms 단위로 증가
 *  2. 각 tick에서 활성화 대상 태스크를 READY로 전환
 *  3. 알고리즘으로 다음 실행 태스크 선택
 *  4. task_func() 호출 (실제 함수 또는 WCET 시뮬레이션)
 *  5. 지터·데드라인 통계 갱신
 *
 * @param s           스케줄러 포인터
 * @param duration_ms 실행 시간 (ms, 0이면 무한 루프)
 * @return 0 정상 종료, -1 오류
 */
int sched_run(scheduler_t *s, uint32_t duration_ms);

/**
 * @brief 스케줄러 강제 종료
 *
 * sched_run() 루프를 다음 tick에서 종료시킵니다.
 * 시그널 핸들러에서 호출 가능합니다.
 *
 * @param s 스케줄러 포인터
 */
void sched_stop(scheduler_t *s);

/**
 * @brief 지터 및 통계 리포트 출력
 *
 * README의 출력 포맷과 동일한 형식으로 결과를 stdout에 출력합니다.
 *
 * @param s 스케줄러 포인터
 */
void sched_print_report(const scheduler_t *s);

/**
 * @brief RMS 스케줄 가능성 검사
 *
 * Liu & Layland 이론: U = Σ(WCET_i / period_i) ≤ n(2^(1/n) − 1)
 *
 * n → ∞ 극한: U ≤ ln(2) ≈ 0.693
 *
 * @param s 스케줄러 포인터
 * @return 1 스케줄 가능, 0 한계 초과 (충분 조건 불만족), -1 오류
 */
int sched_validate_rms(const scheduler_t *s);

/* =========================================================================
 * 동기화 객체 API
 * ========================================================================= */

/**
 * @brief Mutex 생성
 * @param s               스케줄러
 * @param ceiling_priority Priority Ceiling 값
 * @return Mutex ID (1-based), -1 실패
 */
int sched_mutex_create(scheduler_t *s, uint8_t ceiling_priority);

/**
 * @brief Mutex 잠금 (시뮬레이션: 즉시 성공, 충돌 시 -1)
 * @param s        스케줄러
 * @param mutex_id 잠글 Mutex ID
 * @param task_id  잠금 요청 태스크 ID
 * @return 0 성공, -1 이미 잠겨있음
 */
int sched_mutex_lock(scheduler_t *s, uint32_t mutex_id, uint32_t task_id);

/**
 * @brief Mutex 해제
 */
int sched_mutex_unlock(scheduler_t *s, uint32_t mutex_id, uint32_t task_id);

/**
 * @brief 세마포어 생성
 * @param s          스케줄러
 * @param init_count 초기 카운트 값
 * @param max_count  최대 카운트 값
 * @return Semaphore ID (1-based), -1 실패
 */
int sched_sema_create(scheduler_t *s, int32_t init_count, int32_t max_count);

/**
 * @brief 세마포어 Wait (카운트 감소)
 */
int sched_sema_wait(scheduler_t *s, uint32_t sema_id);

/**
 * @brief 세마포어 Signal (카운트 증가)
 */
int sched_sema_signal(scheduler_t *s, uint32_t sema_id);

/* =========================================================================
 * 알고리즘별 내부 함수 (rms.c / edf.c 에서 구현)
 * ========================================================================= */

/**
 * @brief RMS: 등록된 태스크의 우선순위를 주기 기준으로 재배정
 * @param s 스케줄러
 */
void rms_assign_priorities(scheduler_t *s);

/**
 * @brief RMS: 다음 실행할 태스크 선택 (최고 우선순위 READY 태스크)
 * @param s 스케줄러
 * @return 선택된 태스크 인덱스, -1이면 유휴
 */
int rms_select_next(const scheduler_t *s);

/**
 * @brief RMS: CPU 사용률 계산 (0.0 ~ 1.0)
 * @param s 스케줄러
 * @return 사용률 (double)
 */
double rms_utilization(const scheduler_t *s);

/**
 * @brief EDF: 절대 데드라인 갱신
 * @param s          스케줄러
 * @param current_ms 현재 시각 (ms)
 */
void edf_update_deadlines(scheduler_t *s, uint64_t current_ms);

/**
 * @brief EDF: 다음 실행할 태스크 선택 (가장 이른 절대 데드라인)
 * @param s 스케줄러
 * @return 선택된 태스크 인덱스, -1이면 유휴
 */
int edf_select_next(const scheduler_t *s);

#endif /* SENTINEL_SCHEDULER_H */
