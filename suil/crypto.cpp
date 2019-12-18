/* Converted to C by Rusty Russell, based on bitcoin source: */
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>

#include <suil/logging.h>

#include "crypto.h"

namespace suil::crypto {

    void SHA256(SHA256Digest& digest, const void* data, size_t len)
    {
        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, data, len);
        SHA256_Final(&digest.bin(), &ctx);
    }

    void SHA512(SHA512Digest& digest, const void* data, size_t len)
    {
        SHA512_CTX ctx;
        SHA512_Init(&ctx);
        SHA512_Update(&ctx, data, len);
        SHA512_Final(&digest.bin(), &ctx);
    }

    void RIPEMD160(RIPEMD160Digest& digest, const void* data, size_t len)
    {
        RIPEMD160_CTX ctx;
        RIPEMD160_Init(&ctx);
        RIPEMD160_Update(&ctx, data, len);
        RIPEMD160_Final(&digest.bin(), &ctx);
    }

    void Hash256(Hash& hash, const void* data, size_t len)
    {
        SHA256Digest sha;
        crypto::SHA256(sha, data, len);
        crypto::SHA256(hash, &sha.bin(), sha.size());
    }

    void Hash160(HASH160Digest& hash, const void* data, size_t len)
    {
        SHA256Digest sha;
        crypto::SHA256(sha, data, len);
        crypto::RIPEMD160(hash, &sha.bin(), sha.size());
    }

    static suil::String computeBase58(const void* data, size_t len)
    {
        static const char base58Alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

        auto bytes = static_cast<const uint8_t *>(data);
        size_t strLen;
        char *str;
        BN_CTX *ctx;
        BIGNUM *base, *x, *r;
        int i, j;

        strLen = len * 138 / 100 + 2;
        str = (char *)calloc(strLen, sizeof(char));

        ctx = BN_CTX_new();
        BN_CTX_start(ctx);

        base = BN_new();
        x = BN_new();
        r = BN_new();
        BN_set_word(base, 58);
        BN_bin2bn(bytes, len, x);

        i = 0;
        while (!BN_is_zero(x)) {
            BN_div(x, r, x, base, ctx);
            str[i] = base58Alphabet[BN_get_word(r)];
            ++i;
        }
        for (j = 0; j < len; ++j) {
            if (bytes[j] != 0x00) {
                break;
            }
            str[i] = base58Alphabet[0];
            ++i;
        }
        utils::reverse(str, i);

        BN_clear_free(r);
        BN_clear_free(x);
        BN_free(base);
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);

        return suil::String{str, strlen(str), true};
    }

    suil::String toBase58(const void* data, size_t len, bool csum)
    {
        if (csum) {
            Hash hash;
            crypto::Hash256(hash, data, len);
            auto csumed = malloc(len+4);
            auto csumptr = &((uint8_t*)csumed)[len];
            memcpy(csumed, data, len);
            memcpy(csumptr, &hash.cbin(), 4);
            auto tmp = computeBase58(csumed, len+4);
            free(csumed);
            return tmp;
        }
        else {
            return computeBase58(data, len);
        }
    }

    ECKey ECKey::generate()
    {
        EC_KEY *key{nullptr};
        key = EC_KEY_new_by_curve_name(NID_secp256k1);
        if (key == nullptr) {
            serror("allocating secp256k1 curve failed");
            return {};
        }

        if (EC_KEY_generate_key(key) != 1) {
            serror("failed to generate private key: %s", errno_s);
            EC_KEY_free(key);
            return {};
        }
        return {};
    }
}