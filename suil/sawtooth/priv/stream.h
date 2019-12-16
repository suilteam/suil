//
// Created by dc on 2019-12-15.
//

#ifndef SUIL_STREAM_H
#define SUIL_STREAM_H

#include <suil/channel.h>
#include <suil/zmq/zmq.h>

#include "validator.pb.h"

namespace suil::sawsdk {

    struct Message {
        using Type = ::sawtooth::protos::Message::MessageType;
        sptr(::sawtooth::protos::Message);
    };

    struct OnAirMessage final {
        OnAirMessage(const suil::String id);

        OnAirMessage(OnAirMessage&& other);

        OnAirMessage&operator=(OnAirMessage&& other);

        OnAirMessage(const OnAirMessage&) = delete;
        OnAirMessage&operator=(const OnAirMessage&) = delete;

        operator bool() const {
            return Ego.mMessage != nullptr;
        }

        const suil::String& correlationId() const { return mCorrelationId; }

        template <typename T>
        void getMessage(T& proto, Message::Type type) {
            if (Ego.mMessage == nullptr) {
                waitForMessage(type);
            }

            const auto& data = mMessage->content();
            proto.parseFromArray(data.c_str(), data.size());
        }

        void setMessage(Message::UPtr&& message);

        ~OnAirMessage();

    private:
        void waitForMessage(Message::Type type);

        suil::String  mCorrelationId{};
        suil::Channel<bool> mSync{false};
        Message::UPtr mMessage;
        bool  mWaiting{false};
    };
}

#endif // SUIL_STREAM_H
