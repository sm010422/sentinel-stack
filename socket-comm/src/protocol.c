/**
 * @file protocol.c
 * @brief sentinel 프로토콜 직렬화 / 역직렬화 및 CRC-32 구현
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "protocol.h"

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>   /* htonl, htons, ntohl, ntohs */
#include <assert.h>

/* =========================================================================
 * CRC-32 (IEEE 802.3, 다항식 0xEDB88320)
 * ========================================================================= */

/** CRC-32 룩업 테이블 (초기화 후 재사용) */
static uint32_t s_crc_table[256];
static int      s_crc_table_ready = 0;

static void crc32_init_table(void)
{
    uint32_t i;
    uint32_t j;
    uint32_t crc;

    for (i = 0U; i < 256U; i++) {
        crc = i;
        for (j = 0U; j < 8U; j++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
        s_crc_table[i] = crc;
    }
    s_crc_table_ready = 1;
}

uint32_t sentinel_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    size_t   i;

    assert(data != NULL);

    if (s_crc_table_ready == 0) {
        crc32_init_table();
    }

    for (i = 0U; i < len; i++) {
        crc = (crc >> 8) ^ s_crc_table[(crc ^ (uint32_t)data[i]) & 0xFFU];
    }

    return crc ^ 0xFFFFFFFFU;
}

/* =========================================================================
 * 직렬화
 * ========================================================================= */

/**
 * @brief uint64_t를 big-endian 바이트 배열로 기록
 */
static void write_be64(uint8_t *buf, uint64_t val)
{
    buf[0] = (uint8_t)((val >> 56) & 0xFFU);
    buf[1] = (uint8_t)((val >> 48) & 0xFFU);
    buf[2] = (uint8_t)((val >> 40) & 0xFFU);
    buf[3] = (uint8_t)((val >> 32) & 0xFFU);
    buf[4] = (uint8_t)((val >> 24) & 0xFFU);
    buf[5] = (uint8_t)((val >> 16) & 0xFFU);
    buf[6] = (uint8_t)((val >>  8) & 0xFFU);
    buf[7] = (uint8_t)( val        & 0xFFU);
}

/**
 * @brief big-endian 바이트 배열에서 uint64_t 읽기
 */
static uint64_t read_be64(const uint8_t *buf)
{
    return  ((uint64_t)buf[0] << 56)
          | ((uint64_t)buf[1] << 48)
          | ((uint64_t)buf[2] << 40)
          | ((uint64_t)buf[3] << 32)
          | ((uint64_t)buf[4] << 24)
          | ((uint64_t)buf[5] << 16)
          | ((uint64_t)buf[6] <<  8)
          |  (uint64_t)buf[7];
}

int sentinel_packet_serialize(const sentinel_packet_t *pkt,
                               uint8_t *buf, size_t buf_size,
                               size_t *out_len)
{
    size_t   total;
    size_t   offset;
    uint32_t crc;

    if (pkt == NULL || buf == NULL || out_len == NULL) {
        return -1;
    }

    total = SENTINEL_HEADER_SIZE
            + (size_t)pkt->enc_payload_len
            + SENTINEL_AES_TAG_LEN
            + 4U; /* CRC */

    if (buf_size < total) {
        return -1;
    }

    offset = 0U;

    /* ── 헤더 직렬화 (big-endian) ─────────────────────── */
    /* magic (4 bytes) */
    buf[offset++] = (uint8_t)((SENTINEL_MAGIC >> 24) & 0xFFU);
    buf[offset++] = (uint8_t)((SENTINEL_MAGIC >> 16) & 0xFFU);
    buf[offset++] = (uint8_t)((SENTINEL_MAGIC >>  8) & 0xFFU);
    buf[offset++] = (uint8_t)( SENTINEL_MAGIC        & 0xFFU);

    /* version (1 byte) */
    buf[offset++] = pkt->header.version;

    /* msg_type (1 byte) */
    buf[offset++] = pkt->header.msg_type;

    /* seq_num (4 bytes) */
    {
        uint32_t seq = htonl(pkt->header.seq_num);
        (void)memcpy(buf + offset, &seq, 4U);
        offset += 4U;
    }

    /* timestamp_ms (8 bytes) */
    write_be64(buf + offset, pkt->header.timestamp_ms);
    offset += 8U;

    /* src_node_id (1 byte) */
    buf[offset++] = pkt->header.src_node_id;

    /* dst_node_id (1 byte) */
    buf[offset++] = pkt->header.dst_node_id;

    /* topic_id (2 bytes) */
    {
        uint16_t topic = htons(pkt->header.topic_id);
        (void)memcpy(buf + offset, &topic, 2U);
        offset += 2U;
    }

    /* payload_len (4 bytes) */
    {
        uint32_t plen = htonl(pkt->enc_payload_len);
        (void)memcpy(buf + offset, &plen, 4U);
        offset += 4U;
    }

    /* aes_iv (12 bytes) */
    (void)memcpy(buf + offset, pkt->header.aes_iv, SENTINEL_AES_IV_LEN);
    offset += SENTINEL_AES_IV_LEN;

    /* ── 암호화 페이로드 ──────────────────────────────── */
    (void)memcpy(buf + offset, pkt->enc_payload, pkt->enc_payload_len);
    offset += pkt->enc_payload_len;

    /* ── GCM 인증 태그 ────────────────────────────────── */
    (void)memcpy(buf + offset, pkt->aes_tag, SENTINEL_AES_TAG_LEN);
    offset += SENTINEL_AES_TAG_LEN;

    /* ── CRC-32 (헤더 ~ 태그 전체) ───────────────────── */
    crc = sentinel_crc32(buf, offset);
    buf[offset++] = (uint8_t)((crc >> 24) & 0xFFU);
    buf[offset++] = (uint8_t)((crc >> 16) & 0xFFU);
    buf[offset++] = (uint8_t)((crc >>  8) & 0xFFU);
    buf[offset++] = (uint8_t)( crc        & 0xFFU);

    *out_len = offset;
    return 0;
}

/* =========================================================================
 * 역직렬화
 * ========================================================================= */

int sentinel_packet_deserialize(const uint8_t *buf, size_t buf_len,
                                 sentinel_packet_t *pkt)
{
    uint32_t magic;
    uint32_t payload_len;
    uint32_t rx_crc;
    uint32_t calc_crc;
    size_t   offset;
    size_t   expected_total;

    if (buf == NULL || pkt == NULL) {
        return -1;
    }
    if (buf_len < SENTINEL_MIN_PACKET_SIZE) {
        return -1;
    }

    offset = 0U;

    /* ── 매직 검증 ─────────────────────────────────────── */
    magic  = ((uint32_t)buf[0] << 24)
           | ((uint32_t)buf[1] << 16)
           | ((uint32_t)buf[2] <<  8)
           |  (uint32_t)buf[3];

    if (magic != SENTINEL_MAGIC) {
        return -1;
    }
    offset += 4U;

    /* 버전 */
    pkt->header.version  = buf[offset++];
    pkt->header.msg_type = buf[offset++];

    /* seq_num */
    {
        uint32_t seq;
        (void)memcpy(&seq, buf + offset, 4U);
        pkt->header.seq_num = ntohl(seq);
        offset += 4U;
    }

    /* timestamp_ms */
    pkt->header.timestamp_ms = read_be64(buf + offset);
    offset += 8U;

    pkt->header.src_node_id = buf[offset++];
    pkt->header.dst_node_id = buf[offset++];

    /* topic_id */
    {
        uint16_t topic;
        (void)memcpy(&topic, buf + offset, 2U);
        pkt->header.topic_id = ntohs(topic);
        offset += 2U;
    }

    /* payload_len */
    {
        uint32_t plen;
        (void)memcpy(&plen, buf + offset, 4U);
        payload_len = ntohl(plen);
        offset += 4U;
    }

    if (payload_len > SENTINEL_MAX_PAYLOAD) {
        return -1;
    }

    /* aes_iv */
    (void)memcpy(pkt->header.aes_iv, buf + offset, SENTINEL_AES_IV_LEN);
    offset += SENTINEL_AES_IV_LEN;

    /* 전체 패킷 크기 재검증 */
    expected_total = SENTINEL_HEADER_SIZE + (size_t)payload_len
                     + SENTINEL_AES_TAG_LEN + 4U;
    if (buf_len < expected_total) {
        return -1;
    }

    /* CRC-32 검증 (CRC 필드 자체는 제외) */
    rx_crc   = ((uint32_t)buf[expected_total - 4U] << 24)
              | ((uint32_t)buf[expected_total - 3U] << 16)
              | ((uint32_t)buf[expected_total - 2U] <<  8)
              |  (uint32_t)buf[expected_total - 1U];
    calc_crc = sentinel_crc32(buf, expected_total - 4U);

    if (rx_crc != calc_crc) {
        return -1;
    }

    /* 암호화 페이로드 복사 */
    (void)memcpy(pkt->enc_payload, buf + offset, payload_len);
    pkt->enc_payload_len      = payload_len;
    pkt->header.payload_len   = payload_len;
    offset += payload_len;

    /* GCM 태그 복사 */
    (void)memcpy(pkt->aes_tag, buf + offset, SENTINEL_AES_TAG_LEN);

    pkt->crc32 = rx_crc;

    return 0;
}

/* =========================================================================
 * 문자열 변환 유틸리티
 * ========================================================================= */

const char *sentinel_msg_type_str(sentinel_msg_type_t type)
{
    switch (type) {
        case SENTINEL_MSG_DATA:      return "DATA";
        case SENTINEL_MSG_STATUS:    return "STATUS";
        case SENTINEL_MSG_CMD:       return "CMD";
        case SENTINEL_MSG_HEARTBEAT: return "HEARTBEAT";
        case SENTINEL_MSG_ACK:       return "ACK";
        default:                     return "UNKNOWN";
    }
}

const char *sentinel_topic_str(sentinel_topic_id_t topic)
{
    switch (topic) {
        case SENTINEL_TOPIC_SENSOR_DATA: return "SENSOR_DATA";
        case SENTINEL_TOPIC_STATUS:      return "STATUS";
        case SENTINEL_TOPIC_CMD:         return "CMD";
        case SENTINEL_TOPIC_HEARTBEAT:   return "HEARTBEAT";
        default:                         return "UNKNOWN_TOPIC";
    }
}

void sentinel_packet_print(const sentinel_packet_t *pkt)
{
    if (pkt == NULL) {
        return;
    }

    printf("[SENTINEL] magic=0x%08X ver=%u type=%s seq=%u ts=%llu "
           "src=%u dst=%u topic=%s payload_len=%u\n",
           SENTINEL_MAGIC,
           pkt->header.version,
           sentinel_msg_type_str((sentinel_msg_type_t)pkt->header.msg_type),
           pkt->header.seq_num,
           (unsigned long long)pkt->header.timestamp_ms,
           pkt->header.src_node_id,
           pkt->header.dst_node_id,
           sentinel_topic_str((sentinel_topic_id_t)pkt->header.topic_id),
           pkt->enc_payload_len);
}
