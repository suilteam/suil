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

namespace suil::crypto {

    struct SHA256Digest final : Blob<SHA256_DIGEST_LENGTH> {
        using Blob<SHA256_DIGEST_LENGTH>::Blob;

        suil::String toString() const;
        static bool fromString(SHA256Digest& digest, const suil::String& data);
    };

    struct SHA512Digest final : Blob<SHA512_DIGEST_LENGTH> {
        using Blob<SHA512_DIGEST_LENGTH>::Blob;

        suil::String toString() const;
        static bool fromString(SHA512Digest& digest, const suil::String& data);
    };

    struct RIPEMD160Digest final : Blob<RIPEMD160_DIGEST_LENGTH> {
        using Blob<RIPEMD160_DIGEST_LENGTH>::Blob;

        suil::String toString() const;
        static bool fromString(RIPEMD160Digest& digest, const suil::String& data);
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

    struct PublicKey final : Blob<32> {
        using Blob<32>::Blob;
    };
    struct PrivateKey final: Blob<32> {
        using Blob<32>::Blob;
    };

    struct ECKey {
        static ECKey generate();
        static ECKey fromKey(const PrivateKey& key);

        const PrivateKey& getPrivateKey() const;
        const PublicKey& getPublicKey();

        bool isValid() const;

        operator bool() const {return isValid(); }

        void readPrivateKey(PrivateKey& dst);
        void readPublicKey(PublicKey& dst);

    private:
        void generatePublicKey();
        EC_KEY     *ecKey{nullptr};
        PrivateKey  privKey;
        PublicKey   pubKey;
    };
}

#endif //SUIL_ALGS_HPP
