#include "hre/net/crypto/tls13_crypto.h"
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#pragma comment(lib, "bcrypt.lib")

/* ------------------------------------------------------------------ */
/* SHA-256 implementation (FIPS 180-4) for HKDF when BCrypt is       */
/* unavailable on older Windows. We still use BCrypt where possible.  */
/* ------------------------------------------------------------------ */

#define SHA256_BLOCK_SIZE  64
#define SHA256_DIGEST_SIZE 32

struct sha256_ctx {
    uint8_t data[SHA256_BLOCK_SIZE];
    uint32_t datalen;
    unsigned long long bitlen;
    uint32_t state[8];
};

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static void sha256_transform(struct sha256_ctx* ctx, const uint8_t data[]) {
    uint32_t a,b,c,d,e,f,g,h,t1,t2,m[64];
    int i,j;
    for (i=0,j=0; i<16; i++,j+=4)
        m[i] = ((uint32_t)data[j]<<24)|((uint32_t)data[j+1]<<16)|((uint32_t)data[j+2]<<8)|data[j+3];
    for (; i<64; i++)
        m[i] = SIG1(m[i-2])+m[i-7]+SIG0(m[i-15])+m[i-16];
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i=0; i<64; i++) {
        t1 = h + EP1(e) + CH(e,f,g) + sha256_k[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(struct sha256_ctx* ctx) {
    ctx->datalen = 0; ctx->bitlen = 0;
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
}

static void sha256_update(struct sha256_ctx* ctx, const uint8_t data[], size_t len) {
    size_t i;
    for (i=0; i<len; i++) {
        ctx->data[ctx->datalen] = data[i]; ctx->datalen++;
        if (ctx->datalen == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512; ctx->datalen = 0;
        }
    }
}

static void sha256_final(struct sha256_ctx* ctx, uint8_t hash[]) {
    uint32_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < SHA256_BLOCK_SIZE) ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[56+0] = (uint8_t)(ctx->bitlen >> 56);
    ctx->data[56+1] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56+2] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[56+3] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[56+4] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[56+5] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[56+6] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[56+7] = (uint8_t)(ctx->bitlen);
    sha256_transform(ctx, ctx->data);
    for (i=0; i<8; i++) {
        hash[4*i+0] = (uint8_t)(ctx->state[i] >> 24);
        hash[4*i+1] = (uint8_t)(ctx->state[i] >> 16);
        hash[4*i+2] = (uint8_t)(ctx->state[i] >> 8);
        hash[4*i+3] = (uint8_t)(ctx->state[i]);
    }
}

static void sha256(const uint8_t* data, size_t len, uint8_t hash[SHA256_DIGEST_SIZE]) {
    struct sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}

/* ------------------------------------------------------------------ */
/* HMAC-SHA256 (RFC 2104)                                             */
/* ------------------------------------------------------------------ */

static void hmac_sha256(const uint8_t* key, size_t key_len,
                         const uint8_t* data, size_t data_len,
                         uint8_t mac[SHA256_DIGEST_SIZE])
{
    uint8_t k_ipad[SHA256_BLOCK_SIZE];
    uint8_t k_opad[SHA256_BLOCK_SIZE];
    uint8_t tk[SHA256_DIGEST_SIZE];
    size_t i;

    if (key_len > SHA256_BLOCK_SIZE) {
        sha256(key, key_len, tk);
        key = tk;
        key_len = SHA256_DIGEST_SIZE;
    }

    memset(k_ipad, 0, SHA256_BLOCK_SIZE);
    memset(k_opad, 0, SHA256_BLOCK_SIZE);
    memcpy(k_ipad, key, key_len);
    memcpy(k_opad, key, key_len);

    for (i = 0; i < SHA256_BLOCK_SIZE; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }

    {
        struct sha256_ctx ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, k_ipad, SHA256_BLOCK_SIZE);
        sha256_update(&ctx, data, data_len);
        sha256_final(&ctx, mac);
    }

    {
        struct sha256_ctx ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, k_opad, SHA256_BLOCK_SIZE);
        sha256_update(&ctx, mac, SHA256_DIGEST_SIZE);
        sha256_final(&ctx, mac);
    }
}

/* ------------------------------------------------------------------ */
/* HKDF-Extract and HKDF-Expand (RFC 5869)                            */
/* ------------------------------------------------------------------ */

void tls13_hkdf_extract(const uint8_t* salt, size_t salt_len,
                         const uint8_t* ikm, size_t ikm_len,
                         uint8_t* prk)
{
    if (salt == NULL || salt_len == 0) {
        static const uint8_t zero_salt[SHA256_DIGEST_SIZE] = {0};
        hmac_sha256(zero_salt, SHA256_DIGEST_SIZE, ikm, ikm_len, prk);
    } else {
        hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
    }
}

void tls13_hkdf_expand(const uint8_t* prk, size_t prk_len,
                        const uint8_t* info, size_t info_len,
                        uint8_t* okm, size_t okm_len)
{
    uint8_t t[SHA256_DIGEST_SIZE];
    uint8_t block[SHA256_DIGEST_SIZE];
    size_t n = (okm_len + SHA256_DIGEST_SIZE - 1) / SHA256_DIGEST_SIZE;
    size_t i;
    size_t remaining = okm_len;

    for (i = 1; i <= n; i++) {
        struct sha256_ctx ctx;
        sha256_init(&ctx);
        if (i == 1) {
            sha256_update(&ctx, prk, prk_len);
            sha256_update(&ctx, info, info_len);
        } else {
            sha256_update(&ctx, prk, prk_len);
            sha256_update(&ctx, t, SHA256_DIGEST_SIZE);
            sha256_update(&ctx, info, info_len);
        }
        sha256_update(&ctx, &(uint8_t){ (uint8_t)i }, 1);
        sha256_final(&ctx, block);

        size_t copy_len = remaining < SHA256_DIGEST_SIZE ? remaining : SHA256_DIGEST_SIZE;
        memcpy(okm + (i - 1) * SHA256_DIGEST_SIZE, block, copy_len);
        remaining -= copy_len;
        memcpy(t, block, SHA256_DIGEST_SIZE);
    }
}

/* ------------------------------------------------------------------ */
/* AES-128-GCM using BCrypt on Windows                                */
/* ------------------------------------------------------------------ */

static BCRYPT_ALG_HANDLE g_aes_gcm_algo = NULL;
static long g_aes_ref = 0;

static int ensure_aes_gcm(void) {
    if (g_aes_gcm_algo == NULL) {
        NTSTATUS st = BCryptOpenAlgorithmProvider(&g_aes_gcm_algo,
            BCRYPT_AES_ALGORITHM, NULL, 0);
        if (st < 0) return -1;
        st = BCryptSetProperty(g_aes_gcm_algo, BCRYPT_CHAINING_MODE,
            (PUCHAR)BCRYPT_CHAINING_MODE_GCM,
            (ULONG)(sizeof(BCRYPT_CHAINING_MODE_GCM)), 0);
        if (st < 0) return -1;
    }
    return 0;
}

int tls13_encrypt(int cipher, const uint8_t* key, const uint8_t* iv,
                   const uint8_t* plaintext, size_t plaintext_len,
                   const uint8_t* aad, size_t aad_len,
                   uint8_t* ciphertext, uint8_t* tag)
{
    if (cipher == TLS13_AES_128_GCM) {
        if (ensure_aes_gcm() < 0) return -1;

        BCRYPT_KEY_HANDLE hkey = NULL;
        NTSTATUS st = BCryptGenerateSymmetricKey(g_aes_gcm_algo, &hkey, NULL, 0,
            (PUCHAR)key, TLS13_KEY_LEN, 0);
        if (st < 0) return -1;

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO_INIT(auth_info);
        auth_info.pbNonce = (PUCHAR)iv;
        auth_info.cbNonce = TLS13_IV_LEN;
        auth_info.pbAuthData = (PUCHAR)aad;
        auth_info.cbAuthData = (ULONG)aad_len;
        auth_info.pbTag = tag;
        auth_info.cbTag = TLS13_TAG_LEN;

        ULONG result_len = 0;
        st = BCryptEncrypt(hkey, (PUCHAR)plaintext, (ULONG)plaintext_len,
            &auth_info, NULL, 0, ciphertext, (ULONG)plaintext_len,
            &result_len, 0);
        BCryptDestroyKey(hkey);
        return (st < 0) ? -1 : 0;
    }
#if 0
    /* ChaCha20-Poly1305 uses BCrypt CHACHA20_POLY1305 on Windows 10+ */
    else if (cipher == TLS13_CHACHA20_POLY) {
        /* fallback to manual if not available */
        return -1;
    }
#endif
    return -1;
}

int tls13_decrypt(int cipher, const uint8_t* key, const uint8_t* iv,
                   const uint8_t* ciphertext, size_t ciphertext_len,
                   const uint8_t* aad, size_t aad_len,
                   const uint8_t* tag, uint8_t* plaintext)
{
    if (cipher == TLS13_AES_128_GCM) {
        if (ensure_aes_gcm() < 0) return -1;

        BCRYPT_KEY_HANDLE hkey = NULL;
        NTSTATUS st = BCryptGenerateSymmetricKey(g_aes_gcm_algo, &hkey, NULL, 0,
            (PUCHAR)key, TLS13_KEY_LEN, 0);
        if (st < 0) return -1;

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO_INIT(auth_info);
        auth_info.pbNonce = (PUCHAR)iv;
        auth_info.cbNonce = TLS13_IV_LEN;
        auth_info.pbAuthData = (PUCHAR)aad;
        auth_info.cbAuthData = (ULONG)aad_len;
        auth_info.pbTag = (PUCHAR)tag;
        auth_info.cbTag = TLS13_TAG_LEN;

        ULONG result_len = 0;
        st = BCryptDecrypt(hkey, (PUCHAR)ciphertext, (ULONG)ciphertext_len,
            &auth_info, NULL, 0, plaintext, (ULONG)ciphertext_len,
            &result_len, 0);
        BCryptDestroyKey(hkey);
        return (st < 0) ? -1 : 0;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* TLS 1.3 Key Derivation (RFC 8446 §7.1)                             */
/* ------------------------------------------------------------------ */

static const uint8_t k_label_prefix[] = "tls13 ";

void tls13_derive_secret(const uint8_t* prk, size_t prk_len,
                                   const uint8_t* label, size_t label_len,
                                   const uint8_t* transcript_hash,
                                   uint8_t* secret)
{
    /* HKDF-Expand-Label(prk, label, transcript_hash, SHA256_DIGEST_SIZE) */
    size_t hkdflabel_len = 2 + 1 + (sizeof(k_label_prefix)-1) + label_len + 1 +
                           SHA256_DIGEST_SIZE;
    uint8_t* hkdflabel = (uint8_t*)malloc(hkdflabel_len);
    if (!hkdflabel) return;

    size_t off = 0;
    hkdflabel[off++] = (uint8_t)(SHA256_DIGEST_SIZE >> 8);
    hkdflabel[off++] = (uint8_t)SHA256_DIGEST_SIZE;
    hkdflabel[off++] = (uint8_t)((sizeof(k_label_prefix)-1) + label_len);
    memcpy(hkdflabel + off, k_label_prefix, sizeof(k_label_prefix)-1);
    off += sizeof(k_label_prefix)-1;
    memcpy(hkdflabel + off, label, label_len);
    off += label_len;
    memcpy(hkdflabel + off, transcript_hash, SHA256_DIGEST_SIZE);
    off += SHA256_DIGEST_SIZE;

    tls13_hkdf_expand(prk, prk_len, hkdflabel, off, secret, SHA256_DIGEST_SIZE);
    free(hkdflabel);
}

void tls13_derive_key_iv(int cipher, const uint8_t* secret, size_t secret_len,
                          const uint8_t* label, size_t label_len,
                          uint8_t* key, uint8_t* iv)
{
    size_t key_len, iv_len;
    if (cipher == TLS13_AES_128_GCM || cipher == TLS13_CHACHA20_POLY) {
        key_len = TLS13_KEY_LEN;
        iv_len = TLS13_IV_LEN;
    } else {
        return;
    }

    /* Derive key */
    uint8_t* hkdflabel_key = (uint8_t*)malloc(2 + 1 + (sizeof(k_label_prefix)-1) +
                                                label_len + 1 + 0);
    if (!hkdflabel_key) return;
    size_t off = 0;
    hkdflabel_key[off++] = (uint8_t)(key_len >> 8);
    hkdflabel_key[off++] = (uint8_t)key_len;
    hkdflabel_key[off++] = (uint8_t)((sizeof(k_label_prefix)-1) + label_len);
    memcpy(hkdflabel_key + off, k_label_prefix, sizeof(k_label_prefix)-1);
    off += sizeof(k_label_prefix)-1;
    memcpy(hkdflabel_key + off, label, label_len);
    off += label_len;
    /* no context */
    tls13_hkdf_expand(secret, secret_len, hkdflabel_key, off, key, key_len);
    free(hkdflabel_key);

    /* Derive IV */
    uint8_t* hkdflabel_iv = (uint8_t*)malloc(2 + 1 + (sizeof(k_label_prefix)-1) +
                                               label_len + 1 + 0);
    if (!hkdflabel_iv) return;
    off = 0;
    hkdflabel_iv[off++] = (uint8_t)(iv_len >> 8);
    hkdflabel_iv[off++] = (uint8_t)iv_len;
    hkdflabel_iv[off++] = (uint8_t)((sizeof(k_label_prefix)-1) + label_len);
    memcpy(hkdflabel_iv + off, k_label_prefix, sizeof(k_label_prefix)-1);
    off += sizeof(k_label_prefix)-1;
    memcpy(hkdflabel_iv + off, label, label_len);
    off += label_len;
    tls13_hkdf_expand(secret, secret_len, hkdflabel_iv, off, iv, iv_len);
    free(hkdflabel_iv);
}
