//
// Created by Carter Mbotho on 2020-04-08.
//

#ifndef SUIL_PROCESSOR_H
#define SUIL_PROCESSOR_H

#include <suil/sawtooth/dispatcher.h>
#include <suil/sawtooth/transactor.h>

namespace suil::sawsdk {
    
    define_log_tag(SAWSDK_TP);

    struct TransactionProcessor : LOGGER(SAWSDK_TP) {

        TransactionProcessor(String&& validator);

        TransactionProcessor(TransactionProcessor&&) = delete;
        TransactionProcessor(const TransactionProcessor&) = delete;
        TransactionProcessor&operator=(TransactionProcessor&&) = delete;
        TransactionProcessor&operator=(const TransactionProcessor&) = delete;

        zmq::Context& zmqContext() { return mContext; }

        void run();

        TransactionFamily& registerFamily(TransactionFamily::UPtr&& handler);

        template <typename Handler>
        TransactionFamily& registerFamily(const String& name, const String& ns)
        {
            auto family = TransactionFamily::UPtr{new GenericFamily<Handler>(name, ns)};
            return Ego.registerFamily(std::move(family));
        }

    private:
        void registerAll();
        void unRegisterAll();
        void handleRequest(const Data& msg, const String& cid);
        static void connectionMonitor(TransactionProcessor& Self);
        zmq::Context mContext;
        zmq::Pair  mConnMonitor;
        String     mValidator{};
        Dispatcher mDispatcher;
        Stream     mStream;
        Map<TransactionFamily::UPtr> mFamilies;
        bool mRunning{false};
        bool mIsSeverConnected{false};
    };
}
#endif //SUIL_PROCESSOR_H
