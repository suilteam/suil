//
// Created by dc on 2019-12-15.
//

#ifndef SUIL_SDK_H
#define SUIL_SDK_H

#include <suil/base.h>
#include <suil/utils.h>

namespace sawtooth::protos {
    class TransactionHeader;
};

namespace suil::sawsdk {

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

    struct Stream;

    struct GlobalState {
        using KeyValue = std::tuple<suil::String, suil::Data>;
        sptr(GlobalState);

        GlobalState(Stream& stream, const suil::String& contextId);

        virtual const suil::Data getState(const suil::String& address) const;

        virtual void getState(Map<suil::Data>& data, const std::vector<suil::String>& addresses) const;

        virtual void setState(const suil::String& address, const suil::Data& value) const;

        virtual void setState(const std::vector<KeyValue>& data) const;

        virtual void deleteState(const suil::String& address) const;

        virtual void deleteState(const std::vector<suil::String>& addresses) const;

        virtual void addEvent(
                const suil::String& eventType,
                const std::vector<KeyValue>& values,
                const suil::Data& data);

    private:
        Stream& mStream;
        suil::String mContextId{};
    };

    struct Transactor {
        sptr(Transactor);

        Transactor(Transaction&& txn, GlobalState::UPtr&& state)
            : mTransaction(std::move(txn)),
              mState(std::move(state))
        {}

        virtual void apply() = 0;

    private:
        Transaction mTransaction;
        GlobalState::UPtr mState;
    };

    struct TransactionHandler {
        sptr(TransactionHandler);
        virtual suil::String getFamilyName() const;
        virtual std::vector<suil::String> getVersions() const;
        virtual std::vector<suil::String> getNamespaces() const;
        virtual Transactor::Ptr getTransactor(Transaction&& txn, GlobalState::UPtr&& state);
    };

    struct TpContext;
    struct TransactionProcessor {
        sptr(TransactionProcessor);

        TransactionProcessor(suil::String&& connString);

        TransactionProcessor(TransactionProcessor&&) = delete;
        TransactionProcessor(const TransactionProcessor&) = delete;
        TransactionProcessor&operator=(TransactionProcessor&&) = delete;
        TransactionProcessor&operator=(const TransactionProcessor&) = delete;

        virtual void registerHandler(TransactionHandler::UPtr&& handler);
        virtual void run();

    private:
        TpContext* mContext{nullptr};
    };

    suil::Data fromStdString(const std::string& str);

    template <typename T>
    void setValue(T& to, void(T::*func)(const char*, size_t), const suil::Data& data) {
        (to.*func)(reinterpret_cast<const char *>(data.cdata()), data.size());
    }

    template <typename T>
    void setValue(T& to, void(T::*func)(const char*, size_t), const suil::String& data) {
        (to.*func)(data.c_str(), data.size());
    }
}
#endif //SUIL_SDK_H
