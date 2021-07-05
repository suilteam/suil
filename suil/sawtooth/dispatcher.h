//
// Created by dc on 2019-12-16.
//

#ifndef SUIL_DISPATCHER_H
#define SUIL_DISPATCHER_H

#include <suil/sawtooth/stream.h>

namespace suil::sawsdk {
    namespace Client { struct ValidatorContext; }
    define_log_tag(SAWSDK_DISPATCHER);

    struct Dispatcher : LOGGER(SAWSDK_DISPATCHER) {
        sptr(Dispatcher);

        Dispatcher(Dispatcher&&) = delete;
        Dispatcher(const Dispatcher&) = delete;
        Dispatcher&operator=(Dispatcher&&) = delete;
        Dispatcher&operator=(const Dispatcher&) = delete;

        static constexpr Message::Type SERVER_CONNECT_EVENT    = static_cast<Message::Type>(0xFFFE);
        static constexpr Message::Type SERVER_DISCONNECT_EVENT = static_cast<Message::Type>(0XFFFF);

        void connect(const String& validator);
        void bind();

        Stream createStream();

        void disconnect();
        void exit();
    protected:
        friend struct TransactionProcessor;
        friend struct Client::ValidatorContext;
        Dispatcher(zmq::Context& ctx);

    private:
        static const String DISPATCH_THREAD_ENDPOINT;
        static const String SERVER_MONITOR_ENDPOINT;
        static const String EXIT_MESSAGE;

        static void receive(Dispatcher& self);
        static void send(Dispatcher& Self);
        static void exitMonitor(Dispatcher& Self);

        zmq::Context& mContext;
        zmq::Dealer mServerSock;
        zmq::Dealer mMsgSock;
        zmq::Dealer mRequestSock;
        zmq::Pair   mDispatchSock;
        Map<AsyncMessage::Ptr> mOnAirMsgs;
        bool mExiting{false};
        bool mServerConnected{false};
    };

}
#endif //SUIL_DISPATCHER_H
