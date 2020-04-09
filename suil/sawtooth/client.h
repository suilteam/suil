#ifndef SUIL_SAWTOOTH_SDK_H
#define SUIL_SAWTOOTH_SDK_H

#include <suil/base.h>
#include <suil/utils.h>
#include <suil/logging.h>
#include <suil/secp256k1.h>
#include <suil/sawtooth/sdk.h>
#include <suil/sawtooth/protos.h>
#include <suil/json.h>
#include <suil/http/clientapi.h>
#include <suil/sawtooth/common.h>
#include <suil/sawtooth/address.h>


namespace suil::sawsdk::Client {

    define_log_tag(SAWSDK_CLIENT);

    struct Logger final {
        DISABLE_MOVE(Logger);
        DISABLE_COPY(Logger);

        Logger();
        ~Logger();

        inline void operator()(const char *msg, size_t size, log::Level l) {
            Ego.log(msg, size, l);
        }
        inline size_t operator()(char *out, log::Level l, const char *tag, const char *fmt, va_list args) {
            return Ego.format(out, l, tag, fmt, args);
        }
    private:
        void log(const char *msg, size_t size, log::Level l);
        size_t format(char *out, log::Level l, const char *tag, const char *fmt, va_list args);
        static Syslog sSysLog;
    };

    struct Signer final : LOGGER(SAWSDK_CLIENT) {

        Signer(Signer&& other) noexcept;
        Signer& operator=(Signer&& other) noexcept;

        Signer(const Signer&) = delete;
        Signer& operator=(const Signer&) = delete;

        static Signer load(const String& privKey);
        static Signer load(const secp256k1::PrivateKey& privKey);

        void get(secp256k1::PrivateKey& out) const;
        void get(secp256k1::PublicKey& out) const;
        [[nodiscard]]
        const secp256k1::PrivateKey& getPrivateKey() const;
        [[nodiscard]]
        const secp256k1::PublicKey& getPublicKey() const;

        String sign(const void* data, size_t len) const;

        template <typename T>
        String sign(const T& data) const {
            return Ego.sign(data.data(), data.size());
        }

        static bool verify(const void* data, size_t len, const String& signature, const String& publicKey);

        template <typename T>
        static bool verify(const T& data, const String& signature, const String& publicKey) {
            return verify(data.data(), data.size(), signature, publicKey);
        }

        [[nodiscard]]
        inline bool isValid() const { return Ego.mKey.isValid(); }

    private:
        explicit Signer(secp256k1::KeyPair&& key);
        secp256k1::KeyPair mKey;
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

    using Inputs = std::vector<String>;
    using Outputs = std::vector<String>;

    struct Encoder final {
        Encoder(const String& family, String&& familyVersion, const String&& privateKey);

        Transaction operator()(const suil::Data& payload, const Inputs& inputs = {}, const Outputs& outputs = {}) const;

        Batch operator()(const std::vector<Transaction>& txns) const;

        Batch operator()(const Transaction& txn) const {
            return Ego(std::vector<Transaction>{txn});
        }

        static void encode(sawtooth::protos::BatchList& out, const std::vector<Batch>& batches);
        static suil::Data encode(const std::vector<Batch>& batches);
        static void encode(OBuffer& dst, const std::vector<Batch>& batches);
        Batch encode(const suil::Data& payload, const Inputs& inputs = {}, const Outputs& outputs = {});
        void setSigner(const String& key);
        const String& getSigner() const { return Ego.mSignerPublicKey; }
        const String& getBatcher() const { return Ego.mBatcherPublicKey; }
        void setBatcher(const String& key);
    private:
        String mFamily;
        String mFamilyVersion;
        String mBatcherPublicKey;
        String mSignerPublicKey;
        Signer mSigner;
        Signer mBatcher;
    };

    struct HttpRest {
        sptr(HttpRest);

        HttpRest(const String& url,
                 const String& family,
                 const String& familyVersion,
                 const String& privateKey,
                 int port = 8008);

        HttpRest(HttpRest&&) = delete;
        HttpRest(const HttpRest&) = delete;
        HttpRest& operator=(HttpRest&&) = delete;
        HttpRest& operator=(const HttpRest& ) = delete;

        bool asyncBatches(const suil::Data& payload, const StringVec& inputs = {}, const StringVec& outputs = {});
        bool asyncBatches(const std::vector<Batch>& batches);
        suil::Data getState(const String& key, bool encode = true);
        std::vector<suil::Data> getStates(const String& prefix);
        String prefix();
        Encoder& encoder() { return  mEncoder; }
    private:
        Encoder mEncoder;
        AddressEncoder mAddressEncoder;
        http::client::Session mSession;

    private:
        static const char* BATCHES_RESOURCE;
        static const char* STATE_RESOURCE;
    };

    struct Wallet final {
        DISABLE_COPY(Wallet);

        Wallet(Wallet&& other) = default;
        Wallet&operator=(Wallet&&) = default;

        static Wallet open(const String& path, const String& secret);
        static Wallet create(const String& path, const String& secret);
        const String& get(const String& name = "") const;
        const String& generate(const String& name);
        const String& defaultKey() const;
        void save();

        ~Wallet();

    private:
        Wallet(String&& path, String&& secret);

    private:
        typedef decltype(iod::D(
            prop(Name, String),
            prop(Key, String)
        )) Entry;

        typedef decltype(iod::D(
            prop(MasterKey, String),
            prop(Keys,      Repeated<Entry>)
        )) Schema;

        Schema mData;
        String mPath;
        String mSecret;
        bool mDirty{false};
    };
}

#endif