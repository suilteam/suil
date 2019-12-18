//
// Created by dc on 2019-12-16.
//

#ifndef SUIL_TP_H
#define SUIL_TP_H

#include "../sdk.h"
#include "dispatcher.h"

namespace suil::sawsdk {

    define_log_tag(SAWSDK_TP);

    struct TpContext : LOGGER(SAWSDK_TP) {

        TpContext(suil::String&& connString);

        TpContext(TpContext&&) = delete;
        TpContext(const TpContext&) = delete;
        TpContext&operator=(TpContext&&) = delete;
        TpContext&operator=(const TpContext&) = delete;

        zmq::Context& zmqContext() { return mContext; }

        void run();
        void registerHandler(TransactionHandler::UPtr&& handler);

    private:
        void registerAll();
        void unRegisterAll();
        void handleRequest(const suil::Data& msg, const suil::String& cid);
        static void connectionMonitor(TpContext& Self);
        zmq::Context mContext;
        zmq::Pair  mConnMonitor;
        suil::String mConnString{};
        Dispatcher mDispatcher;
        Stream mRespStream;
        suil::Map<TransactionHandler::UPtr> mHandlers;
        bool mRunning{false};
        bool mIsSeverConnected{false};
    };
}
#endif //SUIL_TP_H
