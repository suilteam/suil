#ifndef SUIL_SAWTOOTH_SDK_H
#define SUIL_SAWTOOTH_SDK_H

#include <suil/base.h>
#include <suil/utils.h>
#include <suil/logging.h>
#include <suil/crypto.h>
#include <suil/sawtooth/stream.h>
#include <suil/sawtooth/protos.h>


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
        [[nodiscard]]
        const crypto::PrivateKey& getPrivateKey() const;
        [[nodiscard]]
        const crypto::PublicKey& getPublicKey() const;

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

        [[nodiscard]]
        inline bool isValid() const { return Ego.mKey.isValid(); }

    private:
        explicit Signer(crypto::ECKey&& key);
        crypto::ECKey mKey;
    };

    struct Transaction final {
        sawtooth::protos::Transaction* operator->();
        sawtooth::protos::Transaction& operator* ();
        const sawtooth::protos::Transaction* operator->() const;
        const sawtooth::protos::Transaction& operator* () const;
    private:
        friend struct Encoder;
        Transaction();
        sawtooth::protos::Transaction& get();

        std::shared_ptr<sawtooth::protos::Transaction> mSelf;
    };

    struct Batch final {
        sawtooth::protos::Batch* operator-> ();
        sawtooth::protos::Batch& operator* ();
        const sawtooth::protos::Batch* operator-> () const;
        const sawtooth::protos::Batch& operator* () const;
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

        Transaction operator()(const suil::Data& payload, const Inputs& inputs = {}, const Outputs& outputs = {}) const;

        Batch operator()(const std::vector<Transaction>& txns) const;

        Batch operator()(const Transaction& txn) const {
            return Ego({txn});
        }

        suil::Data operator()(const std::vector<Batch>& batches) const;

        suil::Data operator()(const Batch& batch) const {
            return Ego({batch});
        }

        suil::Data encode(const suil::Data& payload, const Inputs& inputs = {}, const Outputs& outputs = {});

    private:
        suil::String mFamily;
        suil::String mFamilyVersion;
        suil::String mBatcherPublicKey;
        suil::String mSignerPublicKey;
        Signer mSigner;
    };

    struct ValidatorContext;
    struct Validator final {
        using SubmitResp = sawtooth::protos::ClientBatchSubmitResponse;

        Validator();

        Validator(Validator&&) = delete;
        Validator(const Validator&) = delete;
        Validator&operator=(Validator&&) = delete;
        Validator&operator=(const Validator&) = delete;

        bool connect(const suil::String& endpoint);

        OnAirMessage::Ptr submitAsync(const std::vector<Batch>& batches);

        inline OnAirMessage::Ptr submitAsync(const Batch& batch) {
            return Ego.submitAsync({batch});
        }

        SubmitResp submit(const Batch& batch);

    private:
        ValidatorContext *mSelf;
        zmq::Context      mContext{};
    };
}

#endif