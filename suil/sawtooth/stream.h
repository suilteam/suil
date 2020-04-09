//
// Created by dc on 2019-12-15.
//

#ifndef SUIL_STREAM_H
#define SUIL_STREAM_H

#include <suil/channel.h>
#include <suil/zmq/zmq.h>

#include <suil/sawtooth/common.h>

namespace suil::sawsdk {

    struct Message {
        using Type = ::sawtooth::protos::Message::MessageType;
        sptr(::sawtooth::protos::Message);
    };

    struct AsyncMessage final {
        sptr(AsyncMessage);

        AsyncMessage(String id);

        AsyncMessage(AsyncMessage&& other) noexcept;

        AsyncMessage&operator=(AsyncMessage&& other) noexcept;

        AsyncMessage(const AsyncMessage&) = delete;
        AsyncMessage&operator=(const AsyncMessage&) = delete;

        operator bool() const {
            return Ego.mMessage != nullptr;
        }

        [[nodiscard]]
        const String& cid() const { return mCorrelationId; }

        template <typename T>
        void get(T& proto, Message::Type type) {
            if (Ego.mMessage == nullptr) {
                wait(type);
            }

            const auto& data = mMessage->content();
            proto.ParseFromArray(data.c_str(), data.size());
        }

        void set(Message::UPtr&& message);

        ~AsyncMessage();

    private:
        void wait(Message::Type type);

        suil::String  mCorrelationId{};
        suil::Channel<bool> mSync{false};
        Message::UPtr mMessage;
        bool  mWaiting{false};
    };

    struct Stream final : LOGGER(SAWSDK) {
        sptr(Stream);

        Stream(Stream&& other) noexcept;
        Stream&operator=(Stream&& other) noexcept;

        Stream(const Stream&) = delete;
        Stream&operator=(const Stream&) = delete;

        template <typename T>
        AsyncMessage::Ptr sendAsync(Message::Type type, const T& msg) {
            suil::Data data{msg.ByteSizeLong()};
            msg.SerializeToArray(data.data(), data.size());
            return Ego.sendAsync(type, data);
        }

        AsyncMessage::Ptr sendAsync(Message::Type type, const suil::Data& data);

        template <typename T>
        void respond(Message::Type type, const T& msg, const suil::String& correlationId) {
            suil::Data data{msg.ByteSizeLong()};
            msg.SerializeToArray(data.data(), data.size());
            Ego.send(type, data, correlationId);
        }

        void send(Message::Type type, const suil::Data& data, const suil::String& correlationId);

    private:
        friend struct Dispatcher;
        friend struct TpContext;
        Stream(zmq::Context& ctx, suil::Map<AsyncMessage::Ptr>& msgs);

        zmq::Socket mSocket;
        suil::Map<AsyncMessage::Ptr>& mOnAirMsgs;
    };
}

#endif // SUIL_STREAM_H
