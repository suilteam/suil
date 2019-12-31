//
// Created by dc on 2019-12-16.
//

#ifndef SUIL_DISPATCHER_H
#define SUIL_DISPATCHER_H

#include "suil/sawtooth/stream.h"

namespace suil::sawsdk {
    namespace Client { struct ValidatorContext; }
    define_log_tag(SAWSDK_DISPATCHER);

    struct Dispatcher : LOGGER(SAWSDK_DISPATCHER) {
        sptr(Dispatcher);

        Dispatcher(Dispatcher&&) = delete;
        Dispatcher(const Dispatcher&) = delete;
        Dispatcher&operator=(Dispatcher&&) = delete;
        Dispatcher&operator=(const Dispatcher&) = delete;

        static constexpr Message::Type SERVER_CONNECT_EVENT = static_cast<Message::Type>(0xFFFE);
        static constexpr Message::Type SERVER_DISCONNECT_EVENT = static_cast<Message::Type>(0XFFFF);

        void connect(const suil::String& connString);
        void bind();

        Stream createStream();

        void disconnect();
        void exit();
    protected:
        friend struct TpContext;
        friend struct Client::ValidatorContext;
        Dispatcher(zmq::Context& ctx);

    private:
        static const suil::String DISPATCH_THREAD_ENDPOINT;
        static const suil::String SERVER_MONITOR_ENDPOINT;
        static const suil::String EXIT_MESSAGE;

        static void receiveMessages(Dispatcher& self);
        static void sendMessages(Dispatcher& self);
        static void exitMonitor(Dispatcher& Self);

        zmq::Context& mContext;
        zmq::Dealer mServerSock;
        zmq::Dealer mMsgSock;
        zmq::Dealer mRequestSock;
        zmq::Pair   mDispatchSock;
        suil::Map<OnAirMessage::Ptr> mOnAirMessages;
        bool mExiting{false};
        bool mServerConnected{false};
    };

}
#endif //SUIL_DISPATCHER_H
