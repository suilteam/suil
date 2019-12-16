//
// Created by dc on 2019-12-15.
//

#ifndef SUIL_SDK_H
#define SUIL_SDK_H

#include <suil/base.h>

namespace suil::sawtooth {

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

        virtual int getCount(Field field) = 0;
        virtual const suil::Data& getValue(Field field) = 0;
    };

    template <typename T>
    struct Transaction final {
        Transaction(T&& header, suil::Data&& payload, suil::Data&& signature)
            : mHeader(std::move(header)),
              mPayload(std::move(payload)),
              mSignature(std::move(signature))
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
        const suil::Data& payload() const { return mPayload; }
        const suil::Data& signature() const { return mSignature; }

    private:
        TransactionHeader mHeader;
        suil::Data mPayload{};
        suil::Data mSignature{};
    };

    struct GlobalState {
        using KeyValue = std::tuple<suil::String, suil::Data>;
        sptr(GlobalState);

        virtual const suil::Data& getState(const suil::String& address) const;

        virtual Map<suil::Data> getState(const std::vector<suil::String>& addresses) const;

        virtual void setState(const suil::String& address, const suil::Data& value) const;

        virtual void setState(const KeyValue& data) const;

        virtual void deleteState(const suil::String& address) const;

        virtual void deleteState(const std::vector<suil::String>& addresses) const;

        virtual void addEvent(
                const suil::String& eventType,
                const std::vector<KeyValue>& values,
                const suil::Data& data);
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
        virtual suil::String getFamilyName() const = 0;
        virtual std::vector<suil::String> getVersions() const = 0;
        virtual std::vector<suil::String> getNamespaces() const = 0;
        virtual Transactor::Ptr getTransactor(Transaction&& txn, GlobalState::UPtr&& state) = 0;
    };

    struct TransactionProcessor {
    };
}
#endif //SUIL_SDK_H
