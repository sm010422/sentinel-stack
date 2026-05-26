/**
 * @file pubsub.c
 * @brief Publisher-Subscriber 시스템 구현
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "pubsub.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

int pubsub_init(pubsub_t *ps)
{
    if (ps == NULL) {
        return -1;
    }

    (void)memset(ps, 0, sizeof(pubsub_t));
    return 0;
}

int pubsub_find_topic(const pubsub_t *ps, uint16_t topic_id)
{
    uint32_t i;

    if (ps == NULL) {
        return -1;
    }

    for (i = 0U; i < ps->topic_count; i++) {
        if (ps->topics[i].registered && (ps->topics[i].topic_id == topic_id)) {
            return (int)i;
        }
    }

    return -1;
}

int pubsub_register_topic(pubsub_t *ps, uint16_t topic_id)
{
    pubsub_topic_t *t;

    if (ps == NULL) {
        return -1;
    }

    /* 이미 등록된 경우 성공 반환 */
    if (pubsub_find_topic(ps, topic_id) >= 0) {
        return 0;
    }

    if (ps->topic_count >= PUBSUB_MAX_TOPICS) {
        fprintf(stderr, "[PUBSUB] 토픽 풀 포화 (최대 %u)\n", PUBSUB_MAX_TOPICS);
        return -1;
    }

    t = &ps->topics[ps->topic_count];
    (void)memset(t, 0, sizeof(pubsub_topic_t));
    t->topic_id   = topic_id;
    t->registered = true;

    ps->topic_count++;

    printf("[PUBSUB] 토픽 등록: 0x%04X\n", topic_id);
    return 0;
}

int pubsub_subscribe(pubsub_t *ps, uint16_t topic_id,
                     subscriber_cb_t cb, void *userdata)
{
    int             idx;
    pubsub_topic_t *t;
    uint32_t        i;

    if (ps == NULL || cb == NULL) {
        return -1;
    }

    idx = pubsub_find_topic(ps, topic_id);
    if (idx < 0) {
        /* 토픽이 없으면 자동 등록 */
        if (pubsub_register_topic(ps, topic_id) != 0) {
            return -1;
        }
        idx = pubsub_find_topic(ps, topic_id);
        if (idx < 0) {
            return -1;
        }
    }

    t = &ps->topics[idx];

    /* 중복 등록 방지 */
    for (i = 0U; i < t->subscriber_count; i++) {
        if (t->callbacks[i] == cb) {
            return 0; /* 이미 등록됨 */
        }
    }

    if (t->subscriber_count >= PUBSUB_MAX_SUBS) {
        fprintf(stderr, "[PUBSUB] 토픽 0x%04X 구독자 포화\n", topic_id);
        return -1;
    }

    t->callbacks[t->subscriber_count] = cb;
    t->userdata[t->subscriber_count]  = userdata;
    t->subscriber_count++;

    return 0;
}

int pubsub_publish(pubsub_t *ps, uint16_t topic_id,
                   const uint8_t *data, size_t len)
{
    int             idx;
    pubsub_topic_t *t;
    uint32_t        i;
    int             delivered = 0;

    if (ps == NULL || data == NULL) {
        return -1;
    }

    idx = pubsub_find_topic(ps, topic_id);
    if (idx < 0) {
        return 0; /* 구독자 없음 */
    }

    t = &ps->topics[idx];
    t->publish_count++;

    for (i = 0U; i < t->subscriber_count; i++) {
        if (t->callbacks[i] != NULL) {
            t->callbacks[i](topic_id, data, len, t->userdata[i]);
            t->deliver_count++;
            delivered++;
        }
    }

    return delivered;
}

int pubsub_unsubscribe(pubsub_t *ps, uint16_t topic_id, subscriber_cb_t cb)
{
    int             idx;
    pubsub_topic_t *t;
    uint32_t        i;

    if (ps == NULL || cb == NULL) {
        return -1;
    }

    idx = pubsub_find_topic(ps, topic_id);
    if (idx < 0) {
        return -1;
    }

    t = &ps->topics[idx];

    for (i = 0U; i < t->subscriber_count; i++) {
        if (t->callbacks[i] == cb) {
            /* 마지막 항목으로 덮어쓰기 */
            t->subscriber_count--;
            t->callbacks[i] = t->callbacks[t->subscriber_count];
            t->userdata[i]  = t->userdata[t->subscriber_count];
            t->callbacks[t->subscriber_count] = NULL;
            t->userdata[t->subscriber_count]  = NULL;
            return 0;
        }
    }

    return -1;
}

void pubsub_print_stats(const pubsub_t *ps)
{
    uint32_t i;

    if (ps == NULL) {
        return;
    }

    printf("[PUBSUB] === 토픽 통계 ===\n");
    printf("         %-20s %6s %12s %12s\n",
           "토픽 ID", "구독자", "발행 횟수", "전달 횟수");

    for (i = 0U; i < ps->topic_count; i++) {
        const pubsub_topic_t *t = &ps->topics[i];
        printf("         0x%04X %-15s %6u %12llu %12llu\n",
               t->topic_id, "",
               t->subscriber_count,
               (unsigned long long)t->publish_count,
               (unsigned long long)t->deliver_count);
    }
}
