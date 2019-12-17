//
// Created by dc on 2019-12-15.
//

#include "stream.h"

namespace suil::sawsdk {
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
        : mContext{ctx},
          mOnAirMsgs{msgs},
          mSocket{mContext}
    {}

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

        zmq::Message zmsg(out);

        Ego.mSocket.send(zmsg);
    }

    suil::String Stream::getCorrelationId() {
        OBuffer ob;
        ob << (++Ego.mCorrelationCounter);
        return String{ob};
    }
}