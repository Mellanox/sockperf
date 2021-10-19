/*
 *
 * Copyright (c) 2021-2021 Mellanox Technologies Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the Mellanox Technologies Ltd nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */


#include <string>
#include <set>
#include "common.h"
#include "tls.h"

#if defined(DEFINED_TLS)
const char *tls_chipher(const char *chipher) {
    static char tls_chiper_current[256] = TLS_CHIPER_DEFAULT;
    if (chipher) {
        memset(tls_chiper_current, 0, sizeof(tls_chiper_current));
        strncpy(tls_chiper_current, chipher, sizeof(tls_chiper_current) - 1);
    }

    return tls_chiper_current;
}

#if (DEFINED_TLS == 1)

#define IS_TLS_ERR_WANT_RW(e) (SSL_ERROR_WANT_READ == (e) || SSL_ERROR_WANT_WRITE == (e))
#define TLS_WAIT_READ 1
#define TLS_WAIT_WRITE 2
#define TLS_WAIT_WHICH(e)                                                                          \
    (((e) == SSL_ERROR_WANT_READ || (e) == SSL_ERROR_WANT_CONNECT) ? TLS_WAIT_READ : 0) |          \
    (((e) == SSL_ERROR_WANT_WRITE || (e) == SSL_ERROR_WANT_CONNECT) ? TLS_WAIT_WRITE : 0)

#include <openssl/ssl.h>
#include <openssl/err.h>

static bool add_keys_and_certificates(SSL_CTX *ctx);
static bool set_tls_version_and_ciphers(SSL_CTX *ctx);

// This define is only applicable for OpenSSL 3, in OpenSSL 1.1.1 the behaviour
// of unclean shutdown is not strict and does not generate an error.
// So this flag was introduced for OpenSSL 3 as to imitate the old behaviour.
#ifndef SSL_OP_IGNORE_UNEXPECTED_EOF
#define SSL_OP_IGNORE_UNEXPECTED_EOF 0
#endif

static SSL_CTX *ssl_ctx = NULL;

static int wait_for_single_socket(int fd, int which) {
    fd_set read_fds;
    fd_set write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    if (which & TLS_WAIT_READ) FD_SET(fd, &read_fds);

    if (which & TLS_WAIT_WRITE) FD_SET(fd, &write_fds);

    struct timeval timeout_timeval;
    memcpy(&timeout_timeval, g_pApp->m_const_params.select_timeout, sizeof(struct timeval));

    // No exceptfds handling for now.
    int ret = select(fd + 1, &read_fds, &write_fds, NULL, &timeout_timeval);
    if (ret > 0 && !FD_ISSET(fd, &read_fds) && !FD_ISSET(fd, &write_fds)) return 0;

    return ret;
}

int tls_init(void) {
    SSL_CTX *ctx = NULL;

    ssl_ctx = NULL;

    SSL_library_init();
    SSL_load_error_strings();

    if (s_user_params.mode == MODE_SERVER) {
        ctx = SSL_CTX_new(TLS_server_method());
        if (!ctx) {
            log_err("Unable to create SSL context");
            goto error_ctx_not_allocated;
        }

        if (!add_keys_and_certificates(ctx)) {
            log_err("Unable to add keys and certificates");
            goto error_free_ctx;
        }
    } else {
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            log_err("Unable to create SSL context");
            goto error_ctx_not_allocated;
        }

    }

    if  (!set_tls_version_and_ciphers(ctx)) {
        log_err("Unable protocol and cipher");
        goto error_free_ctx;
    }

    ssl_ctx = ctx;

    return SOCKPERF_ERR_NONE;
error_free_ctx:
    SSL_CTX_free(ctx);
error_ctx_not_allocated:
    return SOCKPERF_ERR_FATAL;
}

void tls_exit(void) {
    int ifd;
    if (g_fds_array) {
        for (ifd = 0; ifd <= MAX_FDS_NUM; ifd++) {
            if (g_fds_array[ifd]) {
                SSL_free((SSL *)g_fds_array[ifd]->tls_handle);
            }
        }
    }

    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        EVP_cleanup();
        ssl_ctx = NULL;
    }
}

int tls_connect_or_accept(SSL *ssl) {
    int ret;
    bool try_again;
    auto ssl_func = (SSL_is_server(ssl) ? SSL_accept : SSL_connect);

    do {
        try_again = false;
        ret = ssl_func(ssl);
        if (ret <= 0 && !g_b_exit) {
            int err = SSL_get_error(ssl, ret);
            if (IS_TLS_ERR_WANT_RW(err) || err == SSL_ERROR_WANT_CONNECT) {
                log_dbg("tls_connect_or_accept waiting for socket");

                int rc = wait_for_single_socket(SSL_get_fd(ssl), TLS_WAIT_WHICH(err));
                if (0 > rc && errno != EINTR) {
                    log_err("Failed to wait for event while establishing TLS");
                } else if (!g_b_exit) {
                    try_again = true;
                }
            } else {
                unsigned long e = ERR_peek_error();
                log_err("Failed to establish, TLS-Error: %d Reason: %d, %s, %s", err,
                        ERR_GET_REASON(e), ERR_lib_error_string(e), ERR_reason_error_string(e));
            }
        }
    } while (try_again);

    return ret;
}

void *tls_establish(int fd) {
    SSL *ssl = NULL;

    if (!ssl_ctx) {
        log_err("Failed tls_establish(), no ssl_ctx");
        goto err;
    }

    ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        log_err("Failed SSL_new()");
        goto err;
    }
    if (!SSL_set_fd(ssl, fd)) {
        log_err("Failed SSL_set_fd()");
        goto err;
    }

    if (tls_connect_or_accept(ssl) <= 0) goto err;

    return (void *)ssl;

err:
    if (ssl) {
        SSL_free(ssl);
    }
    return NULL;
}

int tls_write(void *handle, const void *buf, int num) {
    assert(handle);

    return SSL_write((SSL *)handle, buf, num);
}

int tls_read(void *handle, void *buf, int num) {
    assert(handle);

    return SSL_read((SSL *)handle, buf, num);
}

static inline bool add_key_and_certificate(SSL_CTX *ctx, int keytype,
                                           unsigned char *cert, size_t cert_size,
                                           unsigned char *key, size_t key_size)
{
    if (SSL_CTX_use_certificate_ASN1(ctx, cert_size, cert) <= 0){
        log_err("unable to use certificate");
        return false;
    }
    if (SSL_CTX_use_PrivateKey_ASN1(keytype, ctx, key, key_size) <= 0){
        log_err("unable to use key");
        return false;
    }
    return true;
}

static inline EVP_PKEY* generate_EC_pkey_with_NID(int nid=NID_secp384r1)
{
    bool result;
    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_CTX *ctx = nullptr;
    result = ((ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr)) != nullptr) &&
        (EVP_PKEY_keygen_init(ctx) == 1) &&
        (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, nid) > 0) &&
        (EVP_PKEY_keygen(ctx, &pkey) == 1);

    EVP_PKEY_CTX_free(ctx);

    return result ? pkey : nullptr;
}

static inline EVP_PKEY* generate_RSA_pkey(void)
{
    RSA *rsa{nullptr};
    EVP_PKEY *pkey{nullptr};
    BIGNUM *bignum{nullptr};

    bool result =
        ((pkey = EVP_PKEY_new()) != nullptr) &&
        ((bignum = BN_new()) != nullptr) &&
        ((rsa = RSA_new()) != nullptr) &&
        BN_set_word(bignum, RSA_F4) == 1 &&
        RSA_generate_key_ex(rsa, 2048, bignum, nullptr) == 1 &&
        EVP_PKEY_assign(pkey,EVP_PKEY_RSA, rsa) == 1;

    BN_free(bignum);

    if (result) {
        return pkey;
    }

    RSA_free(rsa);
    EVP_PKEY_free(pkey);
    return nullptr;
}

#define X509_ADD_FIELD(field, value) \
    X509_NAME_add_entry_by_txt(name, field, MBSTRING_ASC, value, -1, -1, 0)

X509 * generate_self_signed_x509_with_key(EVP_PKEY * pkey)
{
    X509 * x509;
    X509_NAME * name;
    bool result = ((x509 = X509_new()) != nullptr) &&
        (ASN1_INTEGER_set(X509_get_serialNumber(x509), 1) == 1) &&
        (X509_gmtime_adj(X509_get_notBefore(x509), 0) != nullptr) &&
        (X509_gmtime_adj(X509_get_notAfter(x509), 31536000L) != nullptr) &&
        (X509_set_pubkey(x509, pkey) == 1) &&
        ((name = X509_get_subject_name(x509)) != nullptr) &&
        (X509_ADD_FIELD("C", (unsigned char *)"IL") == 1) &&
        (X509_ADD_FIELD("O", (unsigned char *)"Nvidia") == 1) &&
        (X509_ADD_FIELD("CN", (unsigned char *)"localhost") == 1) &&
        (X509_set_issuer_name(x509, name) == 1) &&
        (X509_sign(x509, pkey, EVP_sha256()) != 0);

    if (result) {
      return x509;
    }
    X509_free(x509);
    return nullptr;
}

#undef X509_ADD_FIELD

static inline bool add_key_and_certificate(SSL_CTX *ctx,
                                           EVP_PKEY *pkey,
                                           X509 *x509)
{
    if ((SSL_CTX_use_certificate(ctx, x509) == 1)
        && (SSL_CTX_use_PrivateKey(ctx, pkey) == 1)) {
        return true;
    } else {
        X509_free(x509);
        EVP_PKEY_free(pkey);
        return false;
    }
}
static inline bool add_rsa2048_key_and_certificate(SSL_CTX *ctx)
{
    EVP_PKEY *pkey;
    X509 *x509;

    return ((pkey = generate_RSA_pkey()) != nullptr) &&
        ((x509 = generate_self_signed_x509_with_key(pkey)) != nullptr) &&
        add_key_and_certificate(ctx, pkey, x509);
}

static inline bool add_ec_sec384r1_key_and_certificate(SSL_CTX *ctx)
{
    EVP_PKEY *pkey;
    X509 *x509;

    return ((pkey = generate_EC_pkey_with_NID()) != nullptr) &&
        ((x509 = generate_self_signed_x509_with_key(pkey)) != nullptr) &&
        add_key_and_certificate(ctx, pkey, x509);
}

static inline bool add_keys_and_certificates(SSL_CTX *ctx)
{
    return add_ec_sec384r1_key_and_certificate(ctx) &&
        add_rsa2048_key_and_certificate(ctx);
}

using cipher_set = std::set<std::string>;
static inline bool set_tls_1_2_version_and_ciphers(SSL_CTX *ctx,
                                                   const char *cipher)
{
    cipher_set tls1_2_ciphers{"AES128-GCM-SHA256",
        "AES256-GCM-SHA384",
        "ECDHE-ECDSA-AES128-GCM-SHA256",
        "ECDHE-ECDSA-AES256-GCM-SHA384",
        "ECDHE-RSA-AES128-GCM-SHA256",
        "ECDHE-RSA-AES256-GCM-SHA384"
            /* "DHE-RSA-AES128-GCM-SHA256", */
            /* "DHE-RSA-AES256-GCM-SHA384", */
    };
    return tls1_2_ciphers.find(cipher) != tls1_2_ciphers.end() &&
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) == 1 &&
        SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION) == 1 &&
        SSL_CTX_set_cipher_list(ctx, cipher) == 1;
}

static inline bool set_tls_1_3_version_and_ciphers(SSL_CTX *ctx,
                                                   const char *cipher)
{
    cipher_set tls1_3_ciphers{"TLS_AES_128_GCM_SHA256",
        "TLS_AES_256_GCM_SHA384"
    };
    return tls1_3_ciphers.find(cipher) != tls1_3_ciphers.end() &&
        SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) == 1 &&
        SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION) == 1 &&
        SSL_CTX_set_ciphersuites(ctx, cipher) == 1;
}

static inline bool set_tls_version_and_ciphers(SSL_CTX *ctx)
{
    std::string cipher{tls_chipher()};
    const char* p_cipher = cipher.c_str();

    if (!set_tls_1_2_version_and_ciphers(ctx, p_cipher) &&
        !set_tls_1_3_version_and_ciphers(ctx, p_cipher)) {
        log_err("Unsupported cipher: %s", p_cipher);
        return false;
    }

    return true;
}
#else
#error Unsupported TLS
#endif

#endif /* DEFINED_TLS */
