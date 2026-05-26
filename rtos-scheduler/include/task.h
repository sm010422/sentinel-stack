/**
 * @file task.h
 * @brief RTOS 태스크 자료구조 및 동기화 객체 정의
 *
 * POSIX 환경에서 실시간 태스크를 추상화하는 구조체와
 * Mutex / Semaphore / Message Queue 정적 구현체를 정의합니다.
 *
 * 설계 기준:
 *  - MISRA-C 2012 준수: 동적 메모리 할당(malloc) 금지
 *  - 정적 메모리 풀 기반 자원 관리
 *  - ARINC 653 파티셔닝 스케줄러 개념 참고
 *
 * @author  Park Sang Min
 * @date    2025
 * @version 1.0
 */

#ifndef SENTINEL_TASK_H
#define SENTINEL_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================================
 * 컴파일 타임 상수
 * ========================================================================= */

/** 스케줄러가 관리할 수 있는 최대 태스크 수 */
#define MAX_TASKS           (32U)

/** 태스크 이름 최대 길이 (null terminator 포함) */
#define TASK_NAME_LEN       (32U)

/** Mutex 풀 크기 */
#define MAX_MUTEXES         (16U)

/** Semaphore 풀 크기 */
#define MAX_SEMAPHORES      (16U)

/** Message Queue 최대 항목 수 */
#define MSG_QUEUE_DEPTH     (32U)

/** Message Queue 최대 메시지 크기 (bytes) */
#define MSG_MAX_SIZE        (256U)

/** 최고 우선순위 값 (낮을수록 우선순위 높음) */
#define TASK_PRIORITY_MAX   (0U)

/** 최저 우선순위 값 */
#define TASK_PRIORITY_MIN   (255U)

/* =========================================================================
 * 태스크 상태 열거형
 * ========================================================================= */

/**
 * @brief 태스크 실행 상태
 *
 * ARINC 653 파티션 상태 모델을 POSIX 시뮬레이션에 맞게 단순화했습니다.
 */
typedef enum {
    TASK_READY      = 0,  /**< 실행 준비 완료, 스케줄러 선택 대기 */
    TASK_RUNNING    = 1,  /**< 현재 CPU 점유 중 */
    TASK_BLOCKED    = 2,  /**< 동기화 객체(Mutex/Sema) 대기 */
    TASK_SUSPENDED  = 3,  /**< 외부에 의해 정지 */
    TASK_DEAD       = 4   /**< 실행 완료 또는 종료 */
} task_state_t;

/* =========================================================================
 * 태스크 구조체
 * ========================================================================= */

/**
 * @brief 실시간 태스크 제어 블록 (TCB)
 *
 * 주기적으로 활성화되는 실시간 태스크를 기술합니다.
 * RMS와 EDF 알고리즘 모두에서 사용하는 공통 구조체입니다.
 *
 * 실시간성 파라미터 관계:
 *   wcet_us <= deadline_ms * 1000 <= period_ms * 1000
 */
typedef struct {
    /* ── 식별 정보 ─────────────────────────────────────── */
    uint32_t     task_id;              /**< 태스크 고유 ID (1-based) */
    char         name[TASK_NAME_LEN];  /**< 태스크 이름 */

    /* ── 실시간 파라미터 ────────────────────────────────── */
    uint32_t     period_ms;            /**< 활성화 주기 (밀리초) */
    uint32_t     deadline_ms;          /**< 상대적 데드라인 (밀리초, period_ms 이하) */
    uint32_t     wcet_us;             /**< 최악 실행 시간 WCET (마이크로초) */
    uint8_t      priority;            /**< 정적 우선순위 (0=최고, 255=최저) */

    /* ── 런타임 상태 ─────────────────────────────────────── */
    task_state_t state;               /**< 현재 실행 상태 */
    uint64_t     next_activation_ms;  /**< 다음 활성화 절대 시간 (ms) */
    uint64_t     abs_deadline_ms;     /**< 현재 주기의 절대 데드라인 (ms) — EDF용 */

    /* ── 태스크 함수 ─────────────────────────────────────── */
    void         (*task_func)(void *arg); /**< 태스크 실행 함수 포인터 */
    void         *arg;                    /**< task_func 에 전달할 인자 */

    /* ── 성능 측정 통계 ──────────────────────────────────── */
    uint64_t     activation_count;    /**< 총 활성화 횟수 */
    uint64_t     deadline_miss_count; /**< 데드라인 미스 횟수 */

    /* Jitter: 실제 활성화 시각 vs 예상 활성화 시각의 차이 (μs) */
    int64_t      last_jitter_us;      /**< 마지막 측정된 지터 값 */
    int64_t      max_jitter_us;       /**< 최대 지터 */
    int64_t      min_jitter_us;       /**< 최소 지터 (음수 가능: 조기 활성화) */
    int64_t      sum_jitter_us;       /**< 평균 계산용 누적 합 */
} rtos_task_t;

/* =========================================================================
 * Mutex (상호 배제 잠금)
 * ========================================================================= */

/**
 * @brief 정적 할당 Mutex
 *
 * 우선순위 역전(Priority Inversion) 방지를 위해
 * Priority Ceiling Protocol 구현 시 ceiling_priority 필드를 사용합니다.
 */
typedef struct {
    uint32_t     mutex_id;            /**< Mutex 고유 ID */
    bool         locked;              /**< 잠금 여부 */
    uint32_t     owner_task_id;       /**< 잠금 보유 태스크 ID (0=없음) */
    uint8_t      ceiling_priority;    /**< Priority Ceiling 값 */
    uint32_t     lock_count;          /**< 재진입(Reentrant) 카운트 */
} rtos_mutex_t;

/* =========================================================================
 * Semaphore (세마포어)
 * ========================================================================= */

/**
 * @brief 정적 할당 계수 세마포어 (Counting Semaphore)
 */
typedef struct {
    uint32_t     sema_id;             /**< Semaphore 고유 ID */
    int32_t      count;               /**< 현재 카운트 값 */
    int32_t      max_count;           /**< 최대 허용 카운트 */
    uint32_t     waiting_tasks[MAX_TASKS]; /**< 대기 중인 태스크 ID 배열 */
    uint32_t     waiting_count;           /**< 대기 태스크 수 */
} rtos_semaphore_t;

/* =========================================================================
 * Message Queue (메시지 큐)
 * ========================================================================= */

/**
 * @brief 메시지 엔트리 (정적 버퍼)
 */
typedef struct {
    uint8_t  data[MSG_MAX_SIZE]; /**< 메시지 페이로드 */
    size_t   len;                /**< 페이로드 길이 */
    uint32_t src_task_id;        /**< 발신 태스크 ID */
    uint64_t timestamp_ms;       /**< 전송 시각 (ms) */
} msg_entry_t;

/**
 * @brief 정적 할당 링버퍼 기반 메시지 큐
 */
typedef struct {
    uint32_t     queue_id;                   /**< 큐 고유 ID */
    msg_entry_t  buffer[MSG_QUEUE_DEPTH];    /**< 정적 메시지 버퍼 */
    uint32_t     head;                        /**< 읽기 인덱스 */
    uint32_t     tail;                        /**< 쓰기 인덱스 */
    uint32_t     count;                       /**< 현재 저장된 메시지 수 */
    uint32_t     overflow_count;              /**< 큐 포화로 인한 드롭 수 */
} rtos_msgqueue_t;

/* =========================================================================
 * 유틸리티 매크로
 * ========================================================================= */

/**
 * @brief 방어적 프로그래밍용 ASSERT 매크로
 *
 * 조건이 거짓일 경우 표준 assert()를 호출합니다.
 * Release 빌드에서는 NDEBUG 정의로 제거됩니다.
 */
#include <assert.h>
#define RTOS_ASSERT(cond)   assert((cond))

/**
 * @brief NULL 포인터 검사 후 에러 리턴
 */
#define CHECK_NULL(ptr)     do { if ((ptr) == NULL) { return -1; } } while (0)

/**
 * @brief 배열 경계 검사
 */
#define CHECK_BOUNDS(idx, max) do { if ((idx) >= (max)) { return -1; } } while (0)

/**
 * @brief task_state_t 를 문자열로 변환
 * @param state 태스크 상태
 * @return 상태 이름 문자열
 */
const char *task_state_str(task_state_t state);

/**
 * @brief rtos_task_t 구조체를 기본값으로 초기화
 * @param task 초기화할 태스크 포인터
 */
void task_init_defaults(rtos_task_t *task);

/**
 * @brief 태스크 파라미터 유효성 검사
 * @param task 검사할 태스크
 * @return 0 = 유효, -1 = 유효하지 않음
 */
int task_validate(const rtos_task_t *task);

#endif /* SENTINEL_TASK_H */
