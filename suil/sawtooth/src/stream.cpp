//
// Created by dc on 2019-12-15.
//

#include "suil/sawtooth/stream.h"

namespace {
    suil::String correlationid() {
        static uint32_t sCounter{0};
        return suil::utils::tostr((++sCounter));
    }
}

namespace suil::sawsdk {

    AsyncMessage::AsyncMessage(const String id)
        : mCorrelationId(std::move(id))
    {}

    AsyncMessage::AsyncMessage(AsyncMessage&& other) noexcept
        : mCorrelationId(std::move(other.mCorrelationId)),
          mSync(std::move(other.mSync)),
          mMessage(std::move(other.mMessage)),
          mWaiting(other.mWaiting)
    {}

    AsyncMessage& AsyncMessage::operator=(AsyncMessage&& other) noexcept {
        mCorrelationId = std::move(other.mCorrelationId);
        mSync = std::move(other.mSync);
        mMessage = std::move(other.mMessage);
        mWaiting = other.mWaiting;
        other.mWaiting = false;
        return Ego;
    }

    AsyncMessage::~AsyncMessage() {
        if (mWaiting) {
            // force waiter to give up on waiting
            !Ego.mSync;
        }
    }

    void AsyncMessage::wait(Message::Type type) {
        while (Ego.mWaiting) {
            // yield until we received a message
            msleep(mnow() + 500);
        }

        if (mMessage == nullptr) {
            mWaiting =  true;
            bool received{false};
            if (!(mSync >> received) || !received) {
                mWaiting = false;
                throw Exception::create("Message '", mCorrelationId, "' was not received");
            }
            mWaiting = false;
        }

        if (mMessage->message_type() != type) {
            throw Exception::create("Unexpected response messafe type, expecing: ",
                    type, ", got: ", mMessage->message_type());
        }
    }

    void AsyncMessage::set(Message::UPtr&& message) {
        Ego.mMessage = std::move(message);
        if (Ego.mWaiting) {
            // notify waiter
            mSync << true;
        }
    }

    Stream::Stream(suil::zmq::Context &ctx, suil::Map<AsyncMessage::Ptr> &msgs)
        : mOnAirMsgs{msgs},
          mSocket{ctx, ZMQ_PUSH}
    {}

    Stream::Stream(Stream&& other) noexcept
        : mOnAirMsgs(other.mOnAirMsgs),
          mSocket(std::move(other.mSocket))
    {}

    Stream& Stream::operator=(suil::sawsdk::Stream &&other) noexcept
    {
        Ego.mOnAirMsgs = other.mOnAirMsgs;
        Ego.mSocket = std::move(other.mSocket);
        return Ego;
    }

    AsyncMessage::Ptr Stream::sendAsync(Message::Type type, const Data &data)
    {
        auto onAir = AsyncMessage::mkshared(correlationid());
        mOnAirMsgs[onAir->cid().peek()] = onAir;
        Ego.send(type, data, onAir->cid());
        return onAir;
    }

    void Stream::send(Message::Type type, const Data &data, const String &cid)
    {
        if (!Ego.mSocket.isConnected()) {
            if (!Ego.mSocket.connect("inproc://send_queue")) {
                throw Exception::create("Failed to connect to send queue: ", zmq_strerror(zmq_errno()));
            }
        }

        ::sawtooth::protos::Message msg;
        msg.set_message_type(type);
        msg.set_correlation_id(cid.data(), cid.size());
        msg.set_content(data.cdata(), data.size());
        Data out{msg.ByteSizeLong()};
        msg.SerializeToArray(out.data(), static_cast<int>(out.size()));

        Ego.mSocket.send(out);
    }
}