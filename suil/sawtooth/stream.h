//
// Created by dc on 2019-12-15.
//

#ifndef SUIL_STREAM_H
#define SUIL_STREAM_H

#include <suil/channel.h>
#include <suil/zmq/zmq.h>

#include <suil/sawtooth/protos.h>

namespace suil::sawsdk {

    struct Message {
        using Type = ::sawtooth::protos::Message::MessageType;
        sptr(::sawtooth::protos::Message);
    };

    struct OnAirMessage final {
        sptr(OnAirMessage);

        OnAirMessage(suil::String id);

        OnAirMessage(OnAirMessage&& other) noexcept;

        OnAirMessage&operator=(OnAirMessage&& other) noexcept;

        OnAirMessage(const OnAirMessage&) = delete;
        OnAirMessage&operator=(const OnAirMessage&) = delete;

        operator bool() const {
            return Ego.mMessage != nullptr;
        }

        [[nodiscard]]
        const suil::String& correlationId() const { return mCorrelationId; }

        template <typename T>
        void getMessage(T& proto, Message::Type type) {
            if (Ego.mMessage == nullptr) {
                waitForMessage(type);
            }

            const auto& data = mMessage->content();
            proto.ParseFromArray(data.c_str(), data.size());
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

    struct Stream {
        sptr(Stream);

        Stream(Stream&& other) noexcept;
        Stream&operator=(Stream&& other) noexcept;

        Stream(const Stream&) = delete;
        Stream&operator=(const Stream&) = delete;

        template <typename T>
        OnAirMessage::Ptr sendMessage(Message::Type type, const T& msg) {
            suil::Data data{msg.ByteSizeLong()};
            msg.SerializeToArray(data.data(), data.size());
            auto correlationId = Ego.getCorrelationId();
            auto onAir = OnAirMessage::mkshared(std::move(correlationId));
            mOnAirMsgs[onAir->correlationId().peek()] = onAir;
            Ego.send(type, data, onAir->correlationId());
            return onAir;
        }

        template <typename T>
        void sendResponse(Message::Type type, const T& msg, const suil::String& correlationId) {
            suil::Data data{msg.ByteSizeLong()};
            msg.SerializeToArray(data.data(), data.size());
            Ego.send(type, data, correlationId);
        }

    private:
        friend struct Dispatcher;
        friend struct TpContext;
        Stream(zmq::Context& ctx, suil::Map<OnAirMessage::Ptr>& msgs);

        suil::String getCorrelationId();

        void send(Message::Type type,
                const suil::Data& data,
                const suil::String& correlationId);

        zmq::Dealer mSocket;
        static uint32_t mCorrelationCounter;
        suil::Map<OnAirMessage::Ptr>& mOnAirMsgs;
    };
}

#endif // SUIL_STREAM_H
