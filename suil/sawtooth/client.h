#ifndef SUIL_SAWTOOTH_SDK_H
#define SUIL_SAWTOOTH_SDK_H

#include <suil/base.h>
#include <suil/utils.h>
#include <suil/logging.h>
#include <suil/crypto.h>

namespace sawtooth::protos {
    class TransactionHeader;
    class Transaction;
    class Batch;
};

namespace suil::sawsdk::Client {

    define_log_tag(SAWSDK_CLIENT);

    struct Signer final : LOGGER(SAWSDK_CLIENT) {

        Signer(Signer&& other) noexcept;
        Signer& operator=(Signer& other) noexcept;

        Signer(const Signer&) = delete;
        Signer& operator=(const Signer&) = delete;

        static Signer load(const suil::String& privKey);
        static Signer load(const crypto::PrivateKey& privKey);

        void get(crypto::PrivateKey& out) const;
        void get(crypto::PublicKey& out) const;
        const crypto::PrivateKey& getPrivateKey() const;
        const crypto::PublicKey getPublicKey() const;

        suil::String sign(const void* data, size_t len) const;

        template <typename T>
        suil::String sign(const T& data) const {
            return Ego.sign(data.data(), data.size());
        }

        static bool verify(const void* data, size_t len, const suil::String& signature, const suil::String& publicKey);

        template <typename T>
        static bool verify(const T& data, const suil::String& signature, const suil::String& publicKey) {
            return verify(data.data(), data.size(), signature, publicKey);
        }

        inline bool isValid() const { return Ego.mKey.isValid(); }

    private:
        Signer(crypto::ECKey&& key);
        crypto::ECKey mKey;
    };

    struct Transaction final {
        sawtooth::protos::Transaction* operator->();
        sawtooth::protos::Transaction& operator* ();
    private:
        friend struct Encoder;
        Transaction();
        sawtooth::protos::Transaction& get();

        std::shared_ptr<sawtooth::protos::Transaction> mSelf;
    };

    struct Batch final {
        sawtooth::protos::Batch* operator-> ();
        sawtooth::protos::Batch& operator* ();
    private:
        friend struct Encoder;
        Batch();
        sawtooth::protos::Batch& get();

        std::shared_ptr<sawtooth::protos::Batch> mSelf{nullptr};
    };

    using Inputs = std::vector<suil::String>;
    using Outputs = std::vector<suil::String>;

    struct Encoder final {
        Encoder(const suil::String& family, const suil::String& familyVersion, const suil::String& privateKey);

        Transaction createTransaction(const suil::Data& payload, const Inputs& inputs = {}, const Outputs& outputs = {}) const;
        Batch createBatch(const std::vector<Transaction>& txns) const;
        Batch createBatch(const Transaction& txn) const {
            return Ego.createBatch({txn});
        }
        suil::Data encode(const Batch& batch);
    private:
        suil::String mFamily;
        suil::String mFamilyVersion;
        suil::String mBatcherPublicKey;
        suil::String mSignerPublicKey;
        Signer mSigner;
    };
}

#endif