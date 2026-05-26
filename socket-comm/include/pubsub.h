/**
 * @file pubsub.h
 * @brief DDS 참고 Publisher-Subscriber 메시지 라우팅 시스템
 *
 * DDS(Data Distribution Service, OMG 표준)의 토픽 기반 라우팅 구조를
 * 단일 노드 내 콜백 디스패치 형태로 단순화하여 구현합니다.
 *
 * 동작 원리:
 *  1. Publisher: pubsub_publish() 호출 → 지정 토픽에 데이터 발행
 *  2. Router:    등록된 토픽의 모든 subscriber 콜백 순서대로 호출
 *  3. Subscriber: 자신이 구독한 토픽의 데이터를 콜백으로 수신
 *
 * 특징:
 *  - 정적 메모리 풀 기반 (malloc 사용 안 함)
 *  - 단일 스레드 모드 (잠금 없음, 이벤트 루프 내 사용)
 *  - 최대 PUBSUB_MAX_TOPICS 개 토픽, 토픽당 PUBSUB_MAX_SUBS 개 구독자
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_PUBSUB_H
#define SENTINEL_PUBSUB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* =========================================================================
 * 용량 상수
 * ========================================================================= */

/** 등록 가능한 최대 토픽 수 */
#define PUBSUB_MAX_TOPICS   16U

/** 토픽당 최대 구독자 수 */
#define PUBSUB_MAX_SUBS     32U

/* =========================================================================
 * 콜백 타입
 * ========================================================================= */

/**
 * @brief 구독자 콜백 함수 타입
 *
 * 토픽에 데이터가 발행되면 이 함수가 호출됩니다.
 *
 * @param topic_id  발행된 토픽 ID
 * @param data      발행된 데이터 (수신 콜백 내에서만 유효)
 * @param len       데이터 길이 (바이트)
 * @param userdata  구독 등록 시 제공한 사용자 데이터 포인터
 */
typedef void (*subscriber_cb_t)(uint16_t topic_id,
                                 const uint8_t *data, size_t len,
                                 void *userdata);

/* =========================================================================
 * 토픽 구조체
 * ========================================================================= */

/**
 * @brief 단일 토픽의 메타데이터 및 구독자 목록
 */
typedef struct {
    uint16_t         topic_id;                       /**< 토픽 ID */
    bool             registered;                     /**< 등록 여부 */
    subscriber_cb_t  callbacks[PUBSUB_MAX_SUBS];     /**< 구독자 콜백 배열 */
    void            *userdata[PUBSUB_MAX_SUBS];      /**< 구독자별 사용자 데이터 */
    uint32_t         subscriber_count;               /**< 현재 구독자 수 */

    /* 통계 */
    uint64_t         publish_count;   /**< 발행 횟수 */
    uint64_t         deliver_count;   /**< 전달 성공 횟수 (콜백 호출 수) */
} pubsub_topic_t;

/* =========================================================================
 * PubSub 시스템 구조체
 * ========================================================================= */

/**
 * @brief Publisher-Subscriber 시스템 (노드 당 하나)
 */
typedef struct {
    pubsub_topic_t topics[PUBSUB_MAX_TOPICS]; /**< 정적 토픽 배열 */
    uint32_t       topic_count;               /**< 등록된 토픽 수 */
} pubsub_t;

/* =========================================================================
 * API
 * ========================================================================= */

/**
 * @brief PubSub 시스템 초기화
 *
 * @param ps PubSub 시스템 포인터
 * @return 0 성공, -1 실패
 */
int pubsub_init(pubsub_t *ps);

/**
 * @brief 토픽 등록
 *
 * 토픽이 이미 등록된 경우 성공(0)을 반환합니다.
 *
 * @param ps       PubSub 시스템
 * @param topic_id 등록할 토픽 ID
 * @return 0 성공, -1 실패 (풀 포화)
 */
int pubsub_register_topic(pubsub_t *ps, uint16_t topic_id);

/**
 * @brief 토픽 구독 등록
 *
 * cb 가 이미 등록된 경우 중복 등록하지 않습니다.
 *
 * @param ps       PubSub 시스템
 * @param topic_id 구독할 토픽 ID
 * @param cb       콜백 함수
 * @param userdata 콜백에 전달할 사용자 데이터
 * @return 0 성공, -1 실패
 */
int pubsub_subscribe(pubsub_t *ps, uint16_t topic_id,
                     subscriber_cb_t cb, void *userdata);

/**
 * @brief 토픽에 데이터 발행
 *
 * 등록된 모든 구독자의 콜백을 동기적으로 호출합니다.
 *
 * @param ps       PubSub 시스템
 * @param topic_id 발행할 토픽 ID
 * @param data     발행할 데이터
 * @param len      데이터 길이
 * @return 콜백이 호출된 구독자 수 (≥0), -1 실패
 */
int pubsub_publish(pubsub_t *ps, uint16_t topic_id,
                   const uint8_t *data, size_t len);

/**
 * @brief 구독 해제
 *
 * @param ps       PubSub 시스템
 * @param topic_id 해제할 토픽 ID
 * @param cb       해제할 콜백 함수
 * @return 0 성공, -1 콜백을 찾지 못함
 */
int pubsub_unsubscribe(pubsub_t *ps, uint16_t topic_id, subscriber_cb_t cb);

/**
 * @brief 토픽 인덱스 조회 (내부 사용)
 *
 * @param ps       PubSub 시스템
 * @param topic_id 조회할 토픽 ID
 * @return 토픽 인덱스 (0-based), -1 미등록
 */
int pubsub_find_topic(const pubsub_t *ps, uint16_t topic_id);

/**
 * @brief 토픽별 통계 출력
 *
 * @param ps PubSub 시스템
 */
void pubsub_print_stats(const pubsub_t *ps);

#endif /* SENTINEL_PUBSUB_H */
