//
// Created by dc on 2019-12-15.
//

#ifndef SUIL_SDK_H
#define SUIL_SDK_H

#include  <type_traits>

#include <suil/base.h>
#include <suil/utils.h>

namespace sawtooth::protos {
    class TransactionHeader;
};

namespace suil::sawsdk {

    struct Errors final {
        static constexpr int InvalidTransaction = 0xFF00;
    };
    struct TransactionHeader {
        enum Field {
            BatcherPublicKey = 1,
            StringDependencies,
            FamilyName,
            FamilyVersion,
            Inputs,
            Nonce,
            Outputs,
            PayloadSha512,
            SignerPublicKey
        };

        TransactionHeader(sawtooth::protos::TransactionHeader* proto);
        TransactionHeader(TransactionHeader&& other) noexcept;
        TransactionHeader& operator=(TransactionHeader&& other) noexcept ;

        TransactionHeader(const TransactionHeader&) = delete;
        TransactionHeader& operator=(const TransactionHeader&) = delete;

        virtual int getCount(Field field);
        virtual suil::Data getValue(Field field, int index = 0);

        virtual ~TransactionHeader();

    private:
        sawtooth::protos::TransactionHeader* mHeader{nullptr};
    };

    struct Transaction final {
        Transaction(TransactionHeader&& header, std::string* payload, std::string* signature)
            : mHeader(std::move(header)),
              mPayload(payload),
              mSignature(signature)
        {}

        Transaction(Transaction&& other)
            : mHeader(std::move(other.mHeader)),
              mPayload(std::move(other.mPayload)),
              mSignature(std::move(mSignature))
        {}

        Transaction&operator=(Transaction&& other) {
            mHeader = std::move(other.mHeader);
            mPayload = std::move(other.mPayload);
            mSignature = std::move(other.mSignature);
            return Ego;
        }

        Transaction(const Transaction&) = delete;
        Transaction&operator=(const Transaction&) = delete;

        const TransactionHeader& header() const { return mHeader; }

        suil::Data payload() const {
            return suil::Data{mPayload->data(), mPayload->size(), false};
        }

        suil::Data signature() const {
            return suil::Data{mSignature->data(), mSignature->size(), false};
        }

    private:
        TransactionHeader mHeader;
        std::unique_ptr<std::string> mPayload{};
        std::unique_ptr<std::string> mSignature{};
    };

    struct GlobalStateContext;

    struct GlobalState {
        using KeyValue = std::tuple<suil::String, suil::Data>;
        sptr(GlobalState);

        GlobalState(GlobalState&& other);
        GlobalState&operator=(GlobalState&& other);

        GlobalState(const GlobalState&) = delete;
        GlobalState&operator=(const GlobalState&) = delete;

        const suil::Data getState(const suil::String& address);

        void getState(Map<suil::Data>& data, const std::vector<suil::String>& addresses);

        void setState(const suil::String& address, const suil::Data& value);

        void setState(const std::vector<KeyValue>& data);

        void deleteState(const suil::String& address);

        void deleteState(const std::vector<suil::String>& addresses);

        void addEvent(
                const suil::String& eventType,
                const std::vector<KeyValue>& values,
                const suil::Data& data);

        ~GlobalState();

    private:
        friend struct TpContext;
        GlobalState(GlobalStateContext* ctx);
        GlobalStateContext *mContext{nullptr};
    };

    struct Transactor {
        sptr(Transactor);

        Transactor(Transaction&& txn, GlobalState&& state)
            : mTxn(std::move(txn)),
              mState(std::move(state))
        {}

        virtual void apply() = 0;

    protected:
        Transaction mTxn;
        GlobalState mState;
    };

    struct TransactionHandler {
        sptr(TransactionHandler);
        virtual suil::String getFamilyName() const = 0;
        virtual std::vector<suil::String> getVersions() const = 0;
        virtual std::vector<suil::String> getNamespaces() const = 0;
        virtual Transactor::Ptr getTransactor(Transaction&& txn, GlobalState&& state) = 0;
    };

    struct TpContext;
    struct TransactionProcessor final {
        sptr(TransactionProcessor);

        TransactionProcessor(suil::String&& connString);

        TransactionProcessor(TransactionProcessor&&) = delete;
        TransactionProcessor(const TransactionProcessor&) = delete;
        TransactionProcessor&operator=(TransactionProcessor&&) = delete;
        TransactionProcessor&operator=(const TransactionProcessor&) = delete;

        virtual void registerHandler(TransactionHandler::UPtr&& handler);
        virtual void run();

        ~TransactionProcessor();

    private:
        TpContext* mContext{nullptr};
    };

    inline suil::Data fromStdString(const std::string& str) {
        return suil::Data{str.data(), str.size(), false};
    }

    template <typename T>
    inline void setValue(T& to, void(T::*func)(const char*, size_t), const suil::Data& data) {
        (to.*func)(reinterpret_cast<const char *>(data.cdata()), data.size());
    }

    template <typename T>
    inline void setValue(T& to, void(T::*func)(const char*, size_t), const suil::String& data) {
        (to.*func)(data.c_str(), data.size());
    }

    template <typename T>
    inline void setValue(T& to, void(T::*func)(const void*, size_t), const suil::Data& data) {
        (to.*func)(data.cdata(), data.size());
    }

    template <typename T>
    inline void setValue(T& to, void(T::*func)(const void*, size_t), const suil::String& data) {
        (to.*func)(data.c_str(), data.size());
    }

    template <typename T>
    inline suil::Data protoSerialize(const T& proto) {
        suil::Data data{proto.ByteSizeLong()};
        proto.SerializeToArray(data.data(), static_cast<int>(data.size()));
        return data;
    }

    #define protoSet(obj, prop, val) \
        setValue(obj, &std::remove_reference_t<decltype(obj)>::set_##prop, val)
    #define protoAdd(obj, prop, val) \
        setValue(obj, &std::remove_reference_t<decltype(obj)>::add_##prop, val)
}
#endif //SUIL_SDK_H
