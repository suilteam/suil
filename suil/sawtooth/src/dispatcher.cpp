//
// Created by dc on 2019-12-16.
//

#include "dispatcher.h"
#include "network.pb.h"
#include "processor.pb.h"

namespace suil::sawsdk {

    const suil::String Dispatcher::DISPATCH_THREAD_ENDPOINT{"inproc://dispatch_thread"};
    const suil::String Dispatcher::SERVER_MONITOR_ENDPOINT{"inproc://server_monitor"};

    Dispatcher::Dispatcher(zmq::Context& ctx)
        : mContext{ctx},
          mServerSock{mContext},
          mMsgSock{mContext},
          mRequestSock{mContext},
          mDispatchSock{mContext},
          mMonitorSock{mContext}
    {
        Ego.mMsgSock.bind("inproc://send_queue");
        Ego.mRequestSock.bind("inproc://request_queue");
        Ego.mDispatchSock.bind(DISPATCH_THREAD_ENDPOINT);

        if (Ego.mServerSock.monitor(SERVER_MONITOR_ENDPOINT, ZMQ_EVENT_CONNECTED | ZMQ_EVENT_DISCONNECTED)) {
            throw Exception::create("Failed to monitor server socket: ", zmq_strerror(zmq_errno()));
        }

        Ego.mMonitorSock.connect(SERVER_MONITOR_ENDPOINT);
        go(receiveMessages(Ego));
        go(sendMessages(Ego));
        go(monitorConnections(Ego));
        go(exitMonitor(Ego));
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
                    zmq::Message rmsg(rdata);
                    Self.mServerSock.send(rmsg);
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

    void Dispatcher::monitorConnections(Dispatcher& Self)
    {
        ldebug(&Self, "starting monitorConnections coroutine");
        while (!Self.mExiting) {
            auto zmsg = Self.mMonitorSock.receive();
            if (zmsg.empty()) {
                ldebug(&Self, "monitorConnections - ignoring empty message");
                continue;
            }

            auto event = *reinterpret_cast<uint16_t*>(zmsg.data());
            if ((Self.mServerConnected && (event & ZMQ_EVENT_DISCONNECTED))||
                (!Self.mServerConnected && (event & ZMQ_EVENT_CONNECTED)))
            {
                Self.mServerConnected = (event & ZMQ_EVENT_CONNECTED) == ZMQ_EVENT_CONNECTED;
                ldebug(&Self, "Server connection state changed to %s",
                        (Self.mServerConnected? "Connected": "Disconnected"));
                sawtooth::protos::Message msg;
                msg.set_message_type((ZMQ_EVENT_CONNECTED & event) == ZMQ_EVENT_CONNECTED ?
                                     SERVER_CONNECT_EVENT : SERVER_DISCONNECT_EVENT);
                suil::Data data{msg.ByteSizeLong()};
                msg.SerializeToArray(data.data(), static_cast<int>(data.size()));
                zmq::Message resp{data};
                Self.mRequestSock.send(resp);
            }
        }
        Self.mMonitorSock.close();
        zmq_socket_monitor(Self.mMonitorSock.raw(), nullptr, ZMQ_EVENT_ALL);
        ldebug(&Self, "Exiting monitorConnections coroutine");
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
                Self.mMonitorSock.close();
                Self.mMsgSock.close();
            }
        }
        ldebug(&Self, "Exiting exitMonitor coroutine");
    }
}