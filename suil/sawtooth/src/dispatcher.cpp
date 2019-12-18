//
// Created by dc on 2019-12-16.
//

#include "dispatcher.h"
#include "network.pb.h"
#include "processor.pb.h"

namespace suil::sawsdk {

    const suil::String Dispatcher::DISPATCH_THREAD_ENDPOINT{"inproc://dispatch_thread"};
    const suil::String Dispatcher::SERVER_MONITOR_ENDPOINT{"inproc://server_monitor"};
    const suil::String Dispatcher::EXIT_MESSAGE{"Exit"};

    Dispatcher::Dispatcher(zmq::Context& ctx)
        : mContext{ctx},
          mServerSock{mContext},
          mMsgSock{mContext},
          mRequestSock{mContext},
          mDispatchSock{mContext}
    {
        Ego.mMsgSock.bind("inproc://send_queue");
        Ego.mRequestSock.bind("inproc://request_queue");
        Ego.mDispatchSock.bind(DISPATCH_THREAD_ENDPOINT);

        go(sendMessages(Ego));
        go(exitMonitor(Ego));
    }

    Stream Dispatcher::createStream() {
        return Stream{Ego.mContext, Ego.mOnAirMessages};
    }

    void Dispatcher::connect(const suil::String &connString) {
        iinfo("Dispatcher connecting to %s", connString());
        bool status{true};
        try {
            status = Ego.mServerSock.connect(connString);
        }
        catch (...) {
            auto ex = Exception::fromCurrent();
            ierror("Connecting to server failed: %s", ex.what());
            throw;
        }

        if (!status) {
            ierror("Connecting to server failed: %s", zmq_strerror(zmq_errno()));
            throw Exception::create("Connecting to server failed: ", zmq_strerror(zmq_errno()));
        }

        if (!Ego.mServerSock.monitor(SERVER_MONITOR_ENDPOINT, ZMQ_EVENT_CONNECTED|ZMQ_EVENT_DISCONNECTED)) {
            throw Exception::create("Failed to monitor server socket: ", zmq_strerror(zmq_errno()));
        }
        go(receiveMessages(Ego));
    }

    void Dispatcher::close() {
        iinfo("Disconnecting server socket");
        Ego.mServerSock.close();
    }

    void Dispatcher::receiveMessages(Dispatcher &Self)
    {
        ldebug(&Self, "starting receiveMessages coroutine");
        while (!Self.mExiting) {
            auto zmsg = Self.mServerSock.receive();
            if (zmsg.empty()) {
                ldebug(&Self, "receivedMessages - ignoring empty message");
                continue;
            }

            Message::UPtr proto(Message::mkunique());
            proto->ParseFromArray(zmsg.data(), static_cast<int>(zmsg.size()));
            ldebug(&Self, "received message {type: %d}", proto->message_type());

            switch (proto->message_type()) {
                case sawtooth::protos::Message_MessageType_TP_PROCESS_REQUEST: {
                    Self.mRequestSock.send(zmsg);
                    break;
                }
                case sawtooth::protos::Message_MessageType_PING_REQUEST: {
                    ldebug(&Self, "Received ping request with correlation %s", proto->correlation_id().data());
                    sawtooth::protos::PingResponse resp;
                    suil::Data data{resp.ByteSizeLong()};
                    resp.SerializeToArray(data.data(), static_cast<int>(data.size()));

                    sawtooth::protos::Message msg;
                    msg.set_correlation_id(proto->correlation_id());
                    msg.set_message_type(sawtooth::protos::Message_MessageType_PING_RESPONSE);
                    msg.set_content(data.cdata(), data.size());
                    suil::Data rdata{msg.ByteSizeLong()};
                    msg.SerializeToArray(data.data(), static_cast<int>(data.size()));
                    Self.mServerSock.send(rdata);
                    break;
                }
                default: {
                    String cid{proto->correlation_id(), false};
                    auto it = Self.mOnAirMessages.find(cid);
                    if (it != Self.mOnAirMessages.end()) {
                        it->second->setMessage(std::move(proto));
                        Self.mOnAirMessages.erase(it);
                    }
                    else {
                        ldebug(&Self, "Received a message with no matching correlation %s", cid());
                    }
                }
            }
        }
        ldebug(&Self, "receive messages coroutine exit");
    }

    void Dispatcher::sendMessages(Dispatcher &Self)
    {
        ldebug(&Self, "Starting sendMessages coroutine");
        while (!Self.mExiting) {
            auto msg = Self.mMsgSock.receive();
            if (msg.empty()) {
                ldebug(&Self, "sendMessages - ignoring empty message");
                continue;
            }

            Self.mServerSock.send(msg);
        }
        ldebug(&Self, "Exiting sendMessages coroutines");
    }

    void Dispatcher::exitMonitor(Dispatcher &Self)
    {
        ldebug(&Self, "Starting exitMonitor coroutine");
        zmq::Pair sock(Self.mContext);
        sock.connect(Dispatcher::DISPATCH_THREAD_ENDPOINT);

        while (!Self.mExiting) {
            auto msg = sock.receive();
            if (msg == EXIT_MESSAGE) {
                Self.mExiting = true;
                Self.mServerSock.close();
                Self.mMsgSock.close();
            }
        }
        ldebug(&Self, "Exiting exitMonitor coroutine");
    }
}