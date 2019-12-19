/* Converted to C by Rusty Russell, based on bitcoin source: */
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>

#include <suil/logging.h>

#include "crypto.h"

namespace suil::crypto {

    constexpr size_t EC_KEY_LEN{32};

    static bool b642bin(uint8_t* bin, size_t len, const void* data, size_t dlen) {
        try {
            auto out = utils::base64::decode(static_cast<const uint8_t *>(data), dlen);
            if (out.size() > len) {
                serror("b642bin - decode destination buffer %d %d", len, out.size());
            }
            memcpy(bin, out.data(), out.size());
        }
        catch (...) {
            auto ex = Exception::fromCurrent();
            serror("b642bin - %s", ex.what());
            return false;
        }
        return true;
    }
    template <typename T>
    static inline bool b642bin(uint8_t* bin, size_t len, const T& data) {
        return b642bin(bin, len, data.data(), data.size());
    }

    static bool hex2bin(uint8_t* bin, size_t len, const String& str)
    {
        try {
            utils::bytes(str, bin, len);
        }
        catch (...) {
            auto ex = Exception::fromCurrent();
            serror("hex2bin - %s", ex.what());
            return false;
        }
        return true;
    }

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

    bool PrivateKey::fromString(PrivateKey &key, const suil::String &str)
    {
        return key.loadString(str);
    }

    bool PublicKey::fromString(PublicKey &key, const suil::String &str) {
        return key.loadString(str);
    }

    ECKey::ECKey(EC_KEY *key)
        : ecKey(key)
    {}

    ECKey::ECKey(suil::crypto::ECKey &&other) noexcept
        : ecKey(other.ecKey)
    {
        other.ecKey = nullptr;
        if (!other.privKey.nil())
            Ego.privKey.copyfrom(other.privKey);
        if (!other.pubKey.nil())
            Ego.pubKey.copyfrom(other.pubKey);
    }

    ECKey& ECKey::operator=(suil::crypto::ECKey &&other) noexcept
    {
        Ego.ecKey = other.ecKey;
        other.ecKey = nullptr;
        if (!other.privKey.nil())
            Ego.privKey.copyfrom(other.privKey);
        if (!other.pubKey.nil())
            Ego.pubKey.copyfrom(other.pubKey);
        return Ego;
    }

    ECKey ECKey::generate()
    {
        EC_KEY *key{nullptr};
        key = EC_KEY_new_by_curve_name(NID_secp256k1);
        if (key == nullptr) {
            serror("generate private key failed: %d %d", __LINE__, ERR_get_error());
            return {};
        }

        if (EC_KEY_generate_key(key) != 1) {
            serror("generate private key failed: %d %d", __LINE__, ERR_get_error());
            EC_KEY_free(key);
            return {};
        }

        auto privateKey = EC_KEY_get0_private_key(key);
        auto keyLen = (size_t) BN_num_bytes(privateKey);
        if (keyLen != EC_KEY_LEN) {
            serror("generate private key failed '%d': %d %d", keyLen, __LINE__, ERR_get_error());
            EC_KEY_free(key);
            return {};
        }
        // default conversion form is compressed
        EC_KEY_set_conv_form(key, POINT_CONVERSION_COMPRESSED);
        auto len = i2o_ECPublicKey(key, nullptr);
        if (len != EC_COMPRESSED_PUBLIC_KEY_SIZE) {
            serror("generate private key failed '%d': %d", len, __LINE__);
            EC_KEY_free(key);
            return {};
        }

        ECKey tmp{key};
        auto dst = &tmp.pubKey.bin();
        i2o_ECPublicKey(key, &dst);
        BN_bn2bin(privateKey, &tmp.privKey.bin());
        return tmp;
    }

    bool ECKey::isValid() const {
        return Ego.ecKey != nullptr;
    }

    const PrivateKey& ECKey::getPrivateKey() const
    {
        return Ego.privKey;
    }

    const PublicKey& ECKey::getPublicKey() const {
        return Ego.pubKey;
    }

    EC_KEY* PublicKey::pub2key(const PublicKey& pub)
    {
        EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp256k1);
        if (key == nullptr) {
            return nullptr;
        }
        EC_KEY_set_conv_form(key, POINT_CONVERSION_COMPRESSED);
        auto bin = &pub.cbin();
        if (o2i_ECPublicKey(&key, &bin, pub.size()) == nullptr) {
            EC_KEY_free(key);
            return nullptr;
        }
        return key;
    }

    ECKey ECKey::fromKey(const suil::crypto::PrivateKey &priv)
    {
        auto key = EC_KEY_new_by_curve_name(NID_secp256k1);
        if (key == nullptr) {
            serror("load private key failure %d %d", __LINE__, ERR_get_error());
            return {};
        }

        auto privateKey = BN_bin2bn(&priv.cbin(), priv.size(), nullptr);
        if (privateKey == nullptr) {
            serror("load private key failure %d %d", __LINE__, ERR_get_error());
            EC_KEY_free(key);
            return {};
        }

        if (!EC_KEY_set_private_key(key, privateKey)) {
            serror("load private key failure %d %d", __LINE__, ERR_get_error());
            BN_clear_free(privateKey);
            EC_KEY_free(key);
            return {};
        }

        auto ctx = BN_CTX_new();
        if (ctx == nullptr) {
            serror("load private key failure %d %d", __LINE__, ERR_get_error());
            BN_clear_free(privateKey);
            EC_KEY_free(key);
            return {};
        }
        BN_CTX_start(ctx);

        auto group = EC_KEY_get0_group(key);
        auto publicKey = EC_POINT_new(group);
        if (publicKey == nullptr) {
            serror("load private key failure %d %d", __LINE__, ERR_get_error());
            BN_CTX_end(ctx);
            BN_CTX_free(ctx);
            BN_clear_free(privateKey);
            EC_KEY_free(key);
            return {};
        }

        if (!EC_POINT_mul(group, publicKey, privateKey, nullptr, nullptr, ctx)) {
            serror("load private key failure %d %d", __LINE__, ERR_get_error());
            EC_POINT_free(publicKey);
            BN_CTX_end(ctx);
            BN_CTX_free(ctx);
            BN_clear_free(privateKey);
            EC_KEY_free(key);
            return {};
        }

        if (!EC_KEY_set_public_key(key, publicKey)) {
            serror("load private key failure %d %d", __LINE__, ERR_get_error());
            EC_POINT_free(publicKey);
            BN_CTX_end(ctx);
            BN_CTX_free(ctx);
            BN_clear_free(privateKey);
            EC_KEY_free(key);
            return {};
        }

        // default conversion form is compressed
        EC_KEY_set_conv_form(key, POINT_CONVERSION_COMPRESSED);
        auto len = i2o_ECPublicKey(key, nullptr);
        if (len != EC_COMPRESSED_PUBLIC_KEY_SIZE) {
            serror("load private key failure '%d': %d", __LINE__);
            EC_POINT_free(publicKey);
            BN_CTX_end(ctx);
            BN_CTX_free(ctx);
            BN_clear_free(privateKey);
            EC_KEY_free(key);
            return {};
        }

        ECKey data(key);
        auto dst = &data.pubKey.bin();
        i2o_ECPublicKey(key, &dst);
        BN_bn2bin(privateKey, &data.privKey.bin());

        EC_POINT_free(publicKey);
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
        BN_clear_free(privateKey);

        return data;
    }

    suil::String ECDSASignature::toCompactForm(bool b64) const
    {
        auto b = &Ego.cbin();
        auto sig= d2i_ECDSA_SIG(nullptr, &b, Ego.size());
        if (sig == nullptr) {
            serror("saving signature failed: %d %d", __LINE__, ERR_get_error());
            return {};
        }
        uint8_t compact[ECDSA_COMPACT_SIGNATURE_SIZE] = {0};
        auto sz = BN_num_bytes(sig->r);
        if (sz > 32) {
            serror("saving signature failed '%d': %d %d", sz, __LINE__, ERR_get_error());
            ECDSA_SIG_free(sig);
            return {};
        }
        // big endian stuff
        BN_bn2bin(sig->r, &compact[32-sz]);

        sz = BN_num_bytes(sig->s);
        if (sz > 32) {
            serror("saving signature failed '%d': %d %d", sz, __LINE__, ERR_get_error());
            ECDSA_SIG_free(sig);
            return {};
        }
        // big endian stuff
        BN_bn2bin(sig->s, &compact[ECDSA_COMPACT_SIGNATURE_SIZE-sz]);
        ECDSA_SIG_free(sig);

        if (b64) {
            return utils::base64::encode(compact, sizeof(compact));
        }
        else {
            return utils::hexstr(compact, sizeof(compact));
        }
    }

    ECDSASignature ECDSASignature::fromCompactForm(const suil::String &sig, bool b64)
    {
        uint8_t compact[ECDSA_COMPACT_SIGNATURE_SIZE] = {0};
        if (b64) {
            if (!b642bin(compact, sizeof(compact), sig)) {
                serror("loading signature failed: %d", __LINE__);
                return {};
            }
        } else {
            if (!hex2bin(compact, sizeof(compact), sig)) {
                serror("loading signature failed: %d", __LINE__);
                return {};
            }
        }

        BIGNUM r{}, s{};
        BN_init(&r); BN_init(&s);
        ECDSA_SIG signature{&r, &s};

        if (BN_bin2bn(compact, 32, signature.r) == nullptr) {
            serror("loading signature failed: %d %d", __LINE__, ERR_get_error());
            BN_clear(&r); BN_clear(&s);
            return {};
        }
        if (BN_bin2bn(&compact[32], 32, signature.s) == nullptr) {
            serror("loading signature failed: %d %d", __LINE__, ERR_get_error());
            BN_clear(&r); BN_clear(&s);
            return {};
        }

        ECDSASignature tmp;
        auto bin = &tmp.bin();
        if (!i2d_ECDSA_SIG(&signature, &bin)) {
            serror("signing message failed - %d %d", __LINE__, ERR_get_error());
            BN_clear(&r); BN_clear(&s);
            return {};
        }
        return tmp;
    }

    bool ECDSASign(ECDSASignature &sig, const PrivateKey &priv, const void *data, size_t len)
    {
        sig.zero();

        auto key = ECKey::fromKey(priv);
        if (!key) {
            serror("signing message failed - invalid key");
            return false;
        }
        Hash hash;
        Hash256(hash, data, len);
        auto signature = ECDSA_do_sign(&hash.bin(), hash.size(), key);
        if (signature == nullptr) {
            serror("signing message failed - %d %d", __LINE__, ERR_get_error());
            return false;
        }


        if (ECDSA_size(key) != sig.size()) {
            serror("signing message failed '%d'- %d", ECDSA_size(key), __LINE__);
            ECDSA_SIG_free(signature);
            return false;
        }
        auto bin = &sig.bin();
        if (!i2d_ECDSA_SIG(signature, &bin)) {
            serror("signing message failed - %d %d", __LINE__, ERR_get_error());
            ECDSA_SIG_free(signature);
            return false;
        }

        ECDSA_SIG_free(signature);
        return true;
    }

    bool ECDSAVerify(const void *data, size_t len, const ECDSASignature &sig, const PublicKey &pub)
    {
        auto key = PublicKey::pub2key(pub);
        if (key == nullptr) {
            serror("verifying signature failed: %d %d", __LINE__, ERR_get_error());
            return false;
        }
        auto bin = &sig.cbin();
        auto signature = d2i_ECDSA_SIG(nullptr, &bin, sig.size());
        if (signature == nullptr) {
            serror("verifying signature failed: %d %d", __LINE__, ERR_get_error());
            EC_KEY_free(key);
            return false;
        }

        Hash hash;
        Hash256(hash, data, len);
        auto verified = ECDSA_do_verify(&hash.bin(), hash.size(), signature, key);
        if (verified < 0) {
            serror("verifying signature failed: %d %d", __LINE__, ERR_get_error());
        }

        ECDSA_SIG_free(signature);
        EC_KEY_free(key);

        return verified == 1;
    }
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("suil::crypto", "[crypto]")
{
    const suil::String priv1Str{"365c872f42c8dfe487c543ec2142d36d843ba31c4cc1152b72ac4052b0792c04"};
    const suil::String pub1Str{"022f931cbec33d538734f926ca7895b16e3ba21a2635fa5481bb50de408723a457"};

    SECTION("Generating/loading key pairs") {
        auto key1 = crypto::ECKey::generate();
        REQUIRE(key1.isValid());

        auto priv1 = key1.getPrivateKey();
        auto key2 = crypto::ECKey::fromKey(priv1);
        REQUIRE(key2.isValid());

        REQUIRE(key2.getPrivateKey() == key1.getPrivateKey());
        REQUIRE(key2.getPublicKey() == key1.getPublicKey());

        WHEN("Loading keys from string") {
            auto priv2 = crypto::PrivateKey::fromString(priv1Str);
            REQUIRE_FALSE(priv2.nil());
            auto pub2 = crypto::PublicKey::fromString(pub1Str);
            REQUIRE_FALSE(priv2.nil());

            auto key3 = crypto::ECKey::fromKey(priv2);
            REQUIRE(key3.isValid());
            printf("Pub: %s\n", key3.getPublicKey().toString()());
            REQUIRE(key3.getPublicKey() == pub2);
            REQUIRE(key3.getPrivateKey() == priv2);
            REQUIRE(pub2.toString() == pub1Str);
            REQUIRE(priv2.toString() == priv1Str());
        }
    }
}

#endif