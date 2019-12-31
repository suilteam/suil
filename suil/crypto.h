//
// Created by dc on 11/26/17.
//

#ifndef SUIL_ALGS_HPP
#define SUIL_ALGS_HPP

#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/ec.h>

#include <suil/utils.h>
#include <suil/blob.h>
#include <suil/zstring.h>

namespace suil::crypto {

    constexpr int ECDSA_SIGNATURE_SIZE{72};
    constexpr int ECDSA_COMPACT_SIGNATURE_SIZE{65};
    constexpr int EC_PRIVATE_KEY_SIZE{32};
    constexpr int EC_COMPRESSED_PUBLIC_KEY_SIZE{33};

    template <size_t N>
    struct Binary : public Blob<N> {
        using Blob<N>::Blob;
        virtual suil::String toString() const {
            return utils::hexstr(&Ego.cbin(), Ego.size());
        }

        virtual bool loadString(const suil::String& str) {
            if (str.size()/2 != N) {
                return false;
            }
            utils::bytes(str, &Ego.bin(), Ego.size());
            return true;
        }
    };

    struct SHA256Digest final : Binary<SHA256_DIGEST_LENGTH> {
        using Binary<SHA256_DIGEST_LENGTH>::Blob;
    };

    struct SHA512Digest final : Binary<SHA512_DIGEST_LENGTH> {
        using Binary<SHA512_DIGEST_LENGTH>::Blob;
    };

    struct RIPEMD160Digest final : Binary<RIPEMD160_DIGEST_LENGTH> {
        using Binary<RIPEMD160_DIGEST_LENGTH>::Blob;
    };

    using Hash = SHA256Digest;
    using HASH160Digest = RIPEMD160Digest;

    void SHA256(SHA256Digest& digest, const void* data, size_t len);

    template <typename T>
    void SHA256(SHA256Digest& digest, const T& data) {
        crypto::SHA256(digest, data.data(), data.size());
    }

    void SHA512(SHA512Digest& digest, const void* data, size_t len);

    template <typename T>
    void SHA512(SHA512Digest& digest, const T& data) {
        crypto::SHA512(digest, data.data(), data.size());
    }

    void RIPEMD160(RIPEMD160Digest& digest, const void* data, size_t len);

    template <typename T>
    void RIPEMD160(RIPEMD160Digest& digest, const T& data) {
        crypto::RIPEMD160(digest, data.data(), data.size());
    }

    void Hash256(Hash& hash, const void* data, size_t len);

    template <typename T>
    void Hash256(Hash& hash, const T& data) {
        crypto::Hash256(hash, data.data(), data.size());
    }

    void Hash160(HASH160Digest& hash, const void* data, size_t len);

    template <typename T>
    void Hash160(HASH160Digest& hash, const T& data) {
        crypto::Hash160(hash, data.data(), data.size());
    }

    suil::String toBase58(const void* data, size_t len, bool csum = false);

    template <typename T>
    suil::String toBase58(const T& data, bool csum = false) {
        return crypto::toBase58(data.data(), data.size());
    }

    template <size_t N>
    static void str2bin(Binary<N>& bin, const suil::String& data) {
        utils::bytes(data, &bin.bin(), bin.size());
    }

    struct PublicKey final : Binary<EC_COMPRESSED_PUBLIC_KEY_SIZE> {
        using Binary<EC_COMPRESSED_PUBLIC_KEY_SIZE>::Blob;

        static bool fromString(PublicKey& key, const String& str);
        static PublicKey fromString(const String& str) {
            PublicKey key;
            fromString(key, str);
            return key;
        }

        static EC_KEY* pub2key(const PublicKey& pub);
    };
    struct PrivateKey final: Binary<EC_PRIVATE_KEY_SIZE> {
        using Binary<EC_PRIVATE_KEY_SIZE>::Blob;

        static bool fromString(PrivateKey& key, const String& str);
        static PrivateKey fromString(const String& str) {
            PrivateKey key;
            fromString(key, str);
            return key;
        }
    };

    struct ECDSASignature;

    struct ECKey final {
        using Conversion = point_conversion_form_t;

        ECKey() = default;

        ECKey(ECKey&& other) noexcept;
        ECKey&operator=(ECKey&& other) noexcept;

        ECKey(const ECKey&) = delete;
        ECKey&operator=(const ECKey&) = delete;

        static ECKey generate();
        static ECKey fromKey(const PrivateKey& key);

        const PrivateKey& getPrivateKey() const;
        const PublicKey& getPublicKey() const;

        bool isValid() const;

        operator bool() const {return isValid(); }

        operator EC_KEY*() const { return ecKey; }

        ~ECKey();

    private:
        ECKey(EC_KEY *key);
        EC_KEY     *ecKey{nullptr};
        PrivateKey  privKey;
        PublicKey   pubKey;
    };

    struct ECDSASignature : public Binary<ECDSA_SIGNATURE_SIZE> {
        using Binary<ECDSA_SIGNATURE_SIZE>::Blob;
        suil::String toCompactForm(bool base64 = true) const;
        static ECDSASignature fromCompactForm(const suil::String& sig, bool b64 = true);
        int8_t RecId{-1};
    };

    bool ECDSASign(ECDSASignature& sig, const PrivateKey& key, const void* data, size_t len);

    template <typename T>
    inline bool ECDSASign(ECDSASignature& sig, const PrivateKey& key, const T& data) {
        return ECDSASign(sig, key, data.data(), data.size());
    }

    inline ECDSASignature ECDSASign(const PrivateKey& key, const void* data, size_t len) {
        ECDSASignature sig;
        ECDSASign(sig, key, data, len);
        return sig;
    }

    template <typename T>
    inline ECDSASignature ECDSASign(const PrivateKey& key, const T& data) {
        return ECDSASign(key, data.data(), data.size());
    }

    bool ECDSAVerify(const void* data, size_t len, const ECDSASignature& sig, const PublicKey& key);

    template <typename T>
    inline bool ECDSAVerify(const T& data, const ECDSASignature& sig, const PublicKey& key) {
        return ECDSAVerify(data.data(), data.size(), sig, key);
    }
}

#endif //SUIL_ALGS_HPP
