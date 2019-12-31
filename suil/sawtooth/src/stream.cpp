//
// Created by dc on 2019-12-15.
//

#include "suil/sawtooth/stream.h"

namespace suil::sawsdk {

    uint32_t Stream::CorrelationCounter{0};

    OnAirMessage::OnAirMessage(const suil::String id)
        : mCorrelationId(std::move(id))
    {}

    OnAirMessage::OnAirMessage(OnAirMessage&& other) noexcept
        : mCorrelationId(std::move(other.mCorrelationId)),
          mSync(std::move(other.mSync)),
          mMessage(std::move(other.mMessage)),
          mWaiting(other.mWaiting)
    {}

    OnAirMessage& OnAirMessage::operator=(OnAirMessage&& other) noexcept {
        mCorrelationId = std::move(other.mCorrelationId);
        mSync = std::move(other.mSync);
        mMessage = std::move(other.mMessage);
        mWaiting = other.mWaiting;
        other.mWaiting = false;
        return Ego;
    }

    OnAirMessage::~OnAirMessage() {
        if (mWaiting) {
            // force waiter to give up on waiting
            !Ego.mSync;
        }
    }

    void OnAirMessage::waitForMessage(Message::Type type) {
        while (mWaiting) {
            // yield until we received a message
            msleep(500);
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

    void OnAirMessage::setMessage(Message::UPtr&& message) {
        Ego.mMessage = std::move(message);
        if (Ego.mWaiting) {
            // notify waiter
            mSync << true;
        }
    }

    Stream::Stream(suil::zmq::Context &ctx, suil::Map<OnAirMessage::Ptr> &msgs)
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

    OnAirMessage::Ptr Stream::asyncSend(Message::Type type, const suil::Data &data)
    {
        auto correlationId = Ego.getCorrelationId();
        auto onAir = OnAirMessage::mkshared(std::move(correlationId));
        mOnAirMsgs[onAir->correlationId().peek()] = onAir;
        Ego.send(type, data, onAir->correlationId());
        return onAir;
    }

    void Stream::send(Message::Type type, const suil::Data &data, const suil::String &correlationId)
    {
        if (!Ego.mSocket.isConnected()) {
            if (!Ego.mSocket.connect("inproc://send_queue")) {
                throw Exception::create("Failed to connect to send queue: ", zmq_strerror(zmq_errno()));
            }
        }

        ::sawtooth::protos::Message msg;
        msg.set_message_type(type);
        msg.set_correlation_id(correlationId.data(), correlationId.size());
        msg.set_content(data.cdata(), data.size());
        suil::Data out{msg.ByteSizeLong()};
        msg.SerializeToArray(out.data(), static_cast<int>(out.size()));

        Ego.mSocket.send(out);
    }

    suil::String Stream::getCorrelationId() {
        OBuffer ob;
        ob << (++Ego.CorrelationCounter);
        return String{ob};
    }
}