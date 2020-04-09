//
// Created by dc on 2019-12-16.
//

#include "../dispatcher.h"
#include "network.pb.h"
#include "processor.pb.h"

namespace suil::sawsdk {

    const String Dispatcher::DISPATCH_THREAD_ENDPOINT{"inproc://dispatch_thread"};
    const String Dispatcher::SERVER_MONITOR_ENDPOINT{"inproc://server_monitor"};
    const String Dispatcher::EXIT_MESSAGE{"Exit"};
    constexpr const char* SEND_QUEUE = "inproc://send_queue";
    constexpr const char* REQUEST_QUEUE = "inproc://request_queue";

    Dispatcher::Dispatcher(zmq::Context& ctx)
        : mContext{ctx},
          mServerSock{mContext},
          mMsgSock{mContext},
          mRequestSock{mContext},
          mDispatchSock{mContext}
    {
        Ego.mMsgSock.bind(SEND_QUEUE);
        Ego.mRequestSock.bind(REQUEST_QUEUE);
        Ego.mDispatchSock.bind(DISPATCH_THREAD_ENDPOINT);
    }

    Stream Dispatcher::createStream() {
        return Stream{Ego.mContext, Ego.mOnAirMsgs};
    }

    void Dispatcher::connect(const String &validator) {
        iinfo("Dispatcher connecting to %s", validator());
        bool status{true};
        try {
            status = Ego.mServerSock.connect(validator);
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

        go(send(Ego));
        go(exitMonitor(Ego));
        go(receive(Ego));
    }

    void Dispatcher::disconnect() {
        iinfo("Disconnecting server socket");
        Ego.mServerSock.close();
    }

    void Dispatcher::exit() {
        idebug("dispatcher exiting...");
        Ego.mDispatchSock.send(Dispatcher::EXIT_MESSAGE);
    }

    void Dispatcher::receive(Dispatcher &S)
    {
        ldebug(&S, "starting receiveMessages coroutine");
        while (!S.mExiting) {
            auto zmsg = S.mServerSock.receive();
            if (zmsg.empty()) {
                ldebug(&S, "receivedMessages - ignoring empty message");
                continue;
            }

            Message::UPtr proto(Message::mkunique());
            proto->ParseFromArray(zmsg.data(), static_cast<int>(zmsg.size()));
            ltrace(&S, "received message {type: %d}", proto->message_type());

            switch (proto->message_type()) {
                case msgtype(TP_PROCESS_REQUEST): {
                    S.mRequestSock.send(zmsg);
                    break;
                }
                case msgtype(PING_REQUEST): {
                    ldebug(&S, "Received ping request with correlation %s", proto->correlation_id().data());
                    sawtooth::protos::PingResponse resp;
                    Data data{resp.ByteSizeLong()};
                    resp.SerializeToArray(data.data(), static_cast<int>(data.size()));

                    sawtooth::protos::Message msg;
                    msg.set_correlation_id(proto->correlation_id());
                    msg.set_message_type(msgtype(PING_RESPONSE));
                    msg.set_content(data.cdata(), data.size());
                    Data rdata{msg.ByteSizeLong()};
                    msg.SerializeToArray(data.data(), static_cast<int>(data.size()));
                    S.mServerSock.send(rdata);
                    break;
                }
                default: {
                    String cid{proto->correlation_id(), false};
                    auto it = S.mOnAirMsgs.find(cid);
                    if (it != S.mOnAirMsgs.end()) {
                        it->second->set(std::move(proto));
                        S.mOnAirMsgs.erase(it);
                    }
                    else {
                        ldebug(&S, "Received a message with no matching correlation %s", cid());
                    }
                }
            }
        }
        ldebug(&S, "receive messages coroutine exit");
    }

    void Dispatcher::send(Dispatcher &S)
    {
        ldebug(&S, "Starting sendMessages coroutine");
        while (!S.mExiting) {
            auto msg = S.mMsgSock.receive();
            if (msg.empty()) {
                ldebug(&S, "sendMessages - ignoring empty message");
                continue;
            }

            if (!S.mServerSock.isConnected()) {
                ldebug(&S, "sendMessages - server is not connected, dropping message");
                continue;
            }

            S.mServerSock.send(msg);
        }
        ldebug(&S, "Exiting sendMessages coroutines");
    }

    void Dispatcher::exitMonitor(Dispatcher &S)
    {
        ldebug(&S, "Starting exitMonitor coroutine");
        zmq::Pair sock(S.mContext);
        sock.connect(Dispatcher::DISPATCH_THREAD_ENDPOINT);

        while (!S.mExiting) {
            auto msg = sock.receive();
            if (msg == EXIT_MESSAGE) {
                S.mExiting = true;
                S.mServerSock.close();
                S.mMsgSock.close();
            }
        }
        ldebug(&S, "Exiting exitMonitor coroutine");
    }
}