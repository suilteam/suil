//
// Created by Carter Mbotho on 2020-04-08.
//

#ifndef SUIL_TRANSACTOR_H
#define SUIL_TRANSACTOR_H

#include <suil/sawtooth/state.h>

namespace suil::sawsdk {

    struct Processor {
        sptr(Processor);

        Processor(Transaction&& txn, GlobalState&& state)
            : mTxn(std::move(txn)),
              mState(std::move(state))
        {}

        virtual void apply() = 0;

    protected:
        Transaction mTxn;
        GlobalState mState;
    };

    struct TransactionFamily {
        sptr(TransactionFamily);

        TransactionFamily(const String& family, const String& ns);

        const String& name() const;
        StringVec& versions();
        StringVec& namespaces();

        virtual Processor::Ptr transactor(Transaction&& txn, GlobalState&& state) = 0;

    protected:
        String    mFamily;
        StringVec mNamespaces;
        StringVec mVersions;
    };

    template <typename Handler>
    struct GenericFamily final : TransactionFamily {
        static_assert(std::is_base_of_v<Processor, Handler>, "'Handler' must implement processor");

        using TransactionFamily::TransactionFamily;

    private:
        friend struct TransactionProcesspr;
        Processor::Ptr transactor(Transaction&& txn, GlobalState&& state) override {
            return Processor::Ptr{new Handler(std::move(txn), std::move(state))};
        }
    };
}
#endif //SUIL_TRANSACTOR_H
