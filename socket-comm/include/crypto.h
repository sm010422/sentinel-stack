/**
 * @file crypto.h
 * @brief AES-256-GCM 페이로드 암호화 인터페이스
 *
 * OpenSSL EVP API를 사용하여 AES-256-GCM 인증 암호화를 제공합니다.
 * GCM 모드는 기밀성(Confidentiality)과 무결성(Integrity)을 동시에 보장합니다.
 *
 * 보안 특성:
 *  - 키 크기: 256 비트 (NIST 승인 암호 강도)
 *  - IV/Nonce: 96 비트 (GCM 권장 크기), 매 패킷마다 새로 생성
 *  - 인증 태그: 128 비트 (NIST SP 800-38D 최대 강도)
 *  - IV 재사용 금지: crypto_generate_iv() 로 항상 무작위 생성
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_CRYPTO_H
#define SENTINEL_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* =========================================================================
 * 암호화 파라미터 상수
 * ========================================================================= */

/** AES-256 키 크기 (바이트) */
#define CRYPTO_KEY_LEN  32U

/** GCM IV/Nonce 크기 (바이트) */
#define CRYPTO_IV_LEN   12U

/** GCM 인증 태그 크기 (바이트) */
#define CRYPTO_TAG_LEN  16U

/* =========================================================================
 * 암호화 컨텍스트
 * ========================================================================= */

/**
 * @brief 암호화 컨텍스트
 *
 * 키는 정적 배열에 저장되며 초기화 이후 변경되지 않습니다.
 * 각 통신 노드는 하나의 컨텍스트를 보유하며, 암호화/복호화 모두 사용합니다.
 */
typedef struct {
    uint8_t  key[CRYPTO_KEY_LEN]; /**< AES-256 비밀 키 */
    bool     initialized;          /**< 초기화 완료 여부 */
} crypto_ctx_t;

/* =========================================================================
 * API
 * ========================================================================= */

/**
 * @brief 암호화 컨텍스트 초기화
 *
 * 제공된 키를 컨텍스트에 복사합니다. key_len 은 반드시 CRYPTO_KEY_LEN 이어야 합니다.
 *
 * @param ctx     초기화할 컨텍스트
 * @param key     AES-256 키 (32 바이트)
 * @param key_len 키 길이 (== CRYPTO_KEY_LEN)
 * @return 0 성공, -1 실패
 */
int crypto_init(crypto_ctx_t *ctx, const uint8_t *key, size_t key_len);

/**
 * @brief AES-256-GCM 암호화
 *
 * 플레인텍스트를 암호화하고 GCM 인증 태그를 생성합니다.
 * IV는 호출 전에 crypto_generate_iv() 로 생성해야 합니다.
 *
 * @param ctx        초기화된 암호화 컨텍스트
 * @param plaintext  평문 버퍼
 * @param pt_len     평문 길이
 * @param iv         AES-GCM IV (CRYPTO_IV_LEN 바이트)
 * @param ciphertext [출력] 암호문 버퍼 (pt_len 이상 크기)
 * @param ct_len     [출력] 암호문 길이 (== pt_len)
 * @param tag        [출력] GCM 인증 태그 (CRYPTO_TAG_LEN 바이트)
 * @return 0 성공, -1 실패
 */
int crypto_encrypt(const crypto_ctx_t *ctx,
                   const uint8_t *plaintext, size_t pt_len,
                   const uint8_t *iv,
                   uint8_t *ciphertext, size_t *ct_len,
                   uint8_t *tag);

/**
 * @brief AES-256-GCM 복호화 및 인증 태그 검증
 *
 * 인증 태그가 일치하지 않으면 복호화를 거부하고 -1 을 반환합니다.
 * 태그 검증 실패 시 출력 버퍼는 유효하지 않습니다.
 *
 * @param ctx        초기화된 암호화 컨텍스트
 * @param ciphertext 암호문 버퍼
 * @param ct_len     암호문 길이
 * @param iv         암호화에 사용된 IV
 * @param tag        검증할 GCM 인증 태그
 * @param plaintext  [출력] 복호화된 평문 버퍼
 * @param pt_len     [출력] 복호화된 평문 길이
 * @return 0 성공, -1 실패 (태그 불일치 포함)
 */
int crypto_decrypt(const crypto_ctx_t *ctx,
                   const uint8_t *ciphertext, size_t ct_len,
                   const uint8_t *iv, const uint8_t *tag,
                   uint8_t *plaintext, size_t *pt_len);

/**
 * @brief 암호학적으로 안전한 IV 생성
 *
 * /dev/urandom 또는 OpenSSL RAND_bytes() 를 사용하여
 * CRYPTO_IV_LEN 바이트의 무작위 IV를 생성합니다.
 *
 * 같은 키로 IV가 재사용되면 GCM 보안이 완전히 무너집니다.
 * 반드시 매 패킷마다 이 함수를 호출하세요.
 *
 * @param iv [출력] 생성된 IV 버퍼 (CRYPTO_IV_LEN 바이트)
 * @return 0 성공, -1 실패
 */
int crypto_generate_iv(uint8_t *iv);

/**
 * @brief 컨텍스트의 키를 메모리에서 안전하게 지우기
 *
 * memset() 은 컴파일러 최적화로 제거될 수 있으므로
 * 플랫폼별 안전 지우기 함수를 사용합니다.
 *
 * @param ctx 지울 컨텍스트
 */
void crypto_clear(crypto_ctx_t *ctx);

#endif /* SENTINEL_CRYPTO_H */
