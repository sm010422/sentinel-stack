/**
 * @file crypto.c
 * @brief AES-256-GCM 암호화 구현 (OpenSSL EVP API)
 *
 * OpenSSL 3.x EVP 인터페이스를 사용합니다.
 * EVP_CIPHER_CTX_new/free 는 라이브러리 내부에서 관리하며
 * 각 함수 내 로컬 범위에서만 사용합니다.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#include "crypto.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

/* =========================================================================
 * 내부 헬퍼
 * ========================================================================= */

static void print_openssl_error(const char *msg)
{
    unsigned long err;
    char          buf[256];

    err = ERR_get_error();
    if (err != 0UL) {
        ERR_error_string_n(err, buf, sizeof(buf));
        fprintf(stderr, "[CRYPTO] %s: %s\n", msg, buf);
    } else {
        fprintf(stderr, "[CRYPTO] %s\n", msg);
    }
}

/* =========================================================================
 * API 구현
 * ========================================================================= */

int crypto_init(crypto_ctx_t *ctx, const uint8_t *key, size_t key_len)
{
    if (ctx == NULL || key == NULL) {
        return -1;
    }
    if (key_len != CRYPTO_KEY_LEN) {
        fprintf(stderr, "[CRYPTO] 키 크기 오류: %zu (필요: %u)\n",
                key_len, CRYPTO_KEY_LEN);
        return -1;
    }

    (void)memcpy(ctx->key, key, CRYPTO_KEY_LEN);
    ctx->initialized = true;
    return 0;
}

int crypto_encrypt(const crypto_ctx_t *ctx,
                   const uint8_t *plaintext, size_t pt_len,
                   const uint8_t *iv,
                   uint8_t *ciphertext, size_t *ct_len,
                   uint8_t *tag)
{
    EVP_CIPHER_CTX *evp_ctx;
    int             len;
    int             ret = -1;

    if (ctx == NULL || !ctx->initialized || plaintext == NULL || iv == NULL ||
        ciphertext == NULL || ct_len == NULL || tag == NULL) {
        return -1;
    }

    evp_ctx = EVP_CIPHER_CTX_new();
    if (evp_ctx == NULL) {
        print_openssl_error("EVP_CIPHER_CTX_new 실패");
        return -1;
    }

    /* AES-256-GCM 초기화 */
    if (EVP_EncryptInit_ex(evp_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        print_openssl_error("EVP_EncryptInit_ex 실패");
        goto cleanup;
    }

    /* IV 길이 설정 (기본 12 바이트) */
    if (EVP_CIPHER_CTX_ctrl(evp_ctx, EVP_CTRL_GCM_SET_IVLEN,
                             (int)CRYPTO_IV_LEN, NULL) != 1) {
        print_openssl_error("IV 길이 설정 실패");
        goto cleanup;
    }

    /* 키와 IV 설정 */
    if (EVP_EncryptInit_ex(evp_ctx, NULL, NULL, ctx->key, iv) != 1) {
        print_openssl_error("키/IV 설정 실패");
        goto cleanup;
    }

    /* 암호화 */
    if (EVP_EncryptUpdate(evp_ctx, ciphertext, &len,
                          plaintext, (int)pt_len) != 1) {
        print_openssl_error("EVP_EncryptUpdate 실패");
        goto cleanup;
    }
    *ct_len = (size_t)len;

    /* 파이널라이즈 (GCM은 패딩 없으므로 추가 출력 없음) */
    if (EVP_EncryptFinal_ex(evp_ctx, ciphertext + *ct_len, &len) != 1) {
        print_openssl_error("EVP_EncryptFinal_ex 실패");
        goto cleanup;
    }
    *ct_len += (size_t)len;

    /* GCM 인증 태그 추출 */
    if (EVP_CIPHER_CTX_ctrl(evp_ctx, EVP_CTRL_GCM_GET_TAG,
                             (int)CRYPTO_TAG_LEN, tag) != 1) {
        print_openssl_error("GCM 태그 추출 실패");
        goto cleanup;
    }

    ret = 0;

cleanup:
    EVP_CIPHER_CTX_free(evp_ctx);
    return ret;
}

int crypto_decrypt(const crypto_ctx_t *ctx,
                   const uint8_t *ciphertext, size_t ct_len,
                   const uint8_t *iv, const uint8_t *tag,
                   uint8_t *plaintext, size_t *pt_len)
{
    EVP_CIPHER_CTX *evp_ctx;
    int             len;
    int             ret = -1;

    if (ctx == NULL || !ctx->initialized || ciphertext == NULL || iv == NULL ||
        tag == NULL || plaintext == NULL || pt_len == NULL) {
        return -1;
    }

    evp_ctx = EVP_CIPHER_CTX_new();
    if (evp_ctx == NULL) {
        print_openssl_error("EVP_CIPHER_CTX_new 실패");
        return -1;
    }

    if (EVP_DecryptInit_ex(evp_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        print_openssl_error("EVP_DecryptInit_ex 실패");
        goto cleanup;
    }

    if (EVP_CIPHER_CTX_ctrl(evp_ctx, EVP_CTRL_GCM_SET_IVLEN,
                             (int)CRYPTO_IV_LEN, NULL) != 1) {
        print_openssl_error("IV 길이 설정 실패");
        goto cleanup;
    }

    if (EVP_DecryptInit_ex(evp_ctx, NULL, NULL, ctx->key, iv) != 1) {
        print_openssl_error("키/IV 설정 실패");
        goto cleanup;
    }

    if (EVP_DecryptUpdate(evp_ctx, plaintext, &len,
                          ciphertext, (int)ct_len) != 1) {
        print_openssl_error("EVP_DecryptUpdate 실패");
        goto cleanup;
    }
    *pt_len = (size_t)len;

    /* GCM 태그 설정 (검증할 태그) */
    if (EVP_CIPHER_CTX_ctrl(evp_ctx, EVP_CTRL_GCM_SET_TAG,
                             (int)CRYPTO_TAG_LEN, (void *)tag) != 1) {
        print_openssl_error("GCM 태그 설정 실패");
        goto cleanup;
    }

    /* 태그 검증 + 파이널라이즈
     * 태그 불일치 시 EVP_DecryptFinal_ex() 가 -1 반환 */
    if (EVP_DecryptFinal_ex(evp_ctx, plaintext + *pt_len, &len) != 1) {
        fprintf(stderr, "[CRYPTO] GCM 태그 검증 실패 — 데이터 위변조 의심\n");
        *pt_len = 0U; /* 출력 버퍼 무효화 */
        goto cleanup;
    }
    *pt_len += (size_t)len;

    ret = 0;

cleanup:
    EVP_CIPHER_CTX_free(evp_ctx);
    return ret;
}

int crypto_generate_iv(uint8_t *iv)
{
    if (iv == NULL) {
        return -1;
    }

    if (RAND_bytes(iv, (int)CRYPTO_IV_LEN) != 1) {
        print_openssl_error("RAND_bytes 실패");
        return -1;
    }

    return 0;
}

void crypto_clear(crypto_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    /* OPENSSL_cleanse: 컴파일러 최적화로 제거되지 않는 안전 지우기 */
    OPENSSL_cleanse(ctx->key, sizeof(ctx->key));
    ctx->initialized = false;
}
