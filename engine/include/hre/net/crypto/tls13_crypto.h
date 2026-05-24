#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TLS 1.3 AEAD algorithms (RFC 8446 §5.1) */
#define TLS13_AES_128_GCM    1
#define TLS13_CHACHA20_POLY  2

/* Key expansion constants (RFC 8446 §7.1) */
#define TLS13_HASH_SHA256_LEN  32
#define TLS13_KEY_LEN          16
#define TLS13_IV_LEN           12
#define TLS13_TAG_LEN          16

/* Derive traffic keys from handshake secrets */
void tls13_derive_key_iv(int cipher, const uint8_t* secret, size_t secret_len,
                         const uint8_t* label, size_t label_len,
                         uint8_t* key, uint8_t* iv);

/* AEAD encrypt (AES-128-GCM or ChaCha20-Poly1305) */
int tls13_encrypt(int cipher, const uint8_t* key, const uint8_t* iv,
                  const uint8_t* plaintext, size_t plaintext_len,
                  const uint8_t* aad, size_t aad_len,
                  uint8_t* ciphertext, uint8_t* tag);

/* AEAD decrypt */
int tls13_decrypt(int cipher, const uint8_t* key, const uint8_t* iv,
                  const uint8_t* ciphertext, size_t ciphertext_len,
                  const uint8_t* aad, size_t aad_len,
                  const uint8_t* tag, uint8_t* plaintext);

/* HKDF-Extract (RFC 5869) using SHA-256 */
void tls13_hkdf_extract(const uint8_t* salt, size_t salt_len,
                        const uint8_t* ikm, size_t ikm_len,
                        uint8_t* prk);

/* HKDF-Expand (RFC 5869) using SHA-256 */
void tls13_hkdf_expand(const uint8_t* prk, size_t prk_len,
                       const uint8_t* info, size_t info_len,
                       uint8_t* okm, size_t okm_len);

/* Derive-Secret (RFC 8446 §7.1) */
void tls13_derive_secret(const uint8_t* prk, size_t prk_len,
                         const uint8_t* label, size_t label_len,
                         const uint8_t* transcript_hash,
                         uint8_t* secret);

#ifdef __cplusplus
}
#endif
