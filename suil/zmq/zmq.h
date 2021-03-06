//
// Created by dc on 2019-12-12.
//

#ifndef SUIL_ZMQ_H
#define SUIL_ZMQ_H

#include <suil/logging.h>
#include <suil/buffer.h>
#include <suil/result.h>

#include <zmq.h>

namespace suil::zmq {

    define_log_tag(ZMQ);

    struct Context : LOGGER(ZMQ) {
        Context();

        Context(Context&& other);
        Context&operator=(Context&& other);

        Context(const Context&) = delete;
        Context&operator=(const Context&) = delete;

        operator void* () { return handle; }

        ~Context();

    private:
        void* handle;
    };

    struct Message {

        Message();

        Message(Message&& other);
        Message&operator=(Message&& other);

        Message(const Message&) = delete;
        Message&operator=(const Message&) = delete;

        void*  data();
        size_t size() const;

        inline bool empty() const {
            return Ego.size() == 0;
        }

        const void* cdata() const;

        operator suil::Data() {
            return suil::Data{Ego.data(), Ego.size(), false};
        }

        inline operator zmq_msg_t*() const {
            return const_cast<zmq_msg_t*>(&msg);
        }

        inline operator bool() const {
            return Ego.empty();
        }

        inline operator suil::String() {
            return suil::String{static_cast<const char*>(Ego.data()), Ego.size(), false};
        }

        void release() { initialized = false; }
        ~Message();

    private:
        friend struct Socket;
        static void destroy(void *data, void *hint);
        zmq_msg_t msg;
        bool      initialized{false};
    };

    struct Socket : LOGGER(ZMQ) {

        Socket(Context& context, int type);
        Socket(Socket&& other);
        Socket&operator=(Socket&& other);

        Socket(const Socket&) = delete;
        Socket&operator=(const Socket&) = delete;

        Message receive(int64_t to = -1);

        bool send(Message& msg, int64_t to = -1);

        bool monitor(const suil::String& endpoint, int events);

        bool send(const void* buf, size_t sz, int64_t to = -1);

        inline bool send(const suil::Data& data, int64_t to = -1) {
            return send(data.cdata(), data.size());
        }

        inline bool send(const suil::OBuffer& data, int64_t to = -1) {
            return send(data.data(), data.size(), to);
        }

        inline bool send(const suil::String& data, int64_t to = -1) {
            return send(data.data(), data.size(), to);
        }

        inline void* raw() { return sock; }

        bool bind(const suil::String& endoint);

        bool connect(const suil::String& endpoint);

        bool isConnected() const;

        void close();

        virtual ~Socket();

    protected:
        bool resolveSocket();
        void setIdentity();
        void* sock{nullptr};
        Context& ctx;
        int type{ZMQ_REQ};
        suil::String id{};
    private:
        int  fd{-1};
    };

    struct Requestor: public Socket {
        Requestor(Context& context, int type = ZMQ_REQ);

        Requestor(Requestor&& other);
        Requestor&operator=(Requestor&& other);

        Requestor(const Requestor& other) = delete;
        Requestor&operator=(const Requestor& other) = delete;
    };

    struct Responder: public Socket {
        Responder(Context& context);

        Responder(Responder&& other);
        Responder&operator=(Responder&& other);

        Responder(const Responder& other) = delete;
        Responder&operator=(const Responder& other) = delete;
    };

    struct Dealer: public Socket {
        sptr(Dealer);

        Dealer(Context& context);
        Dealer(Dealer&& other);
        Dealer&operator=(Dealer&& other);

        Dealer(const Dealer& other) = delete;
        Dealer&operator=(const Dealer& other) = delete;
    };

    struct Pair: public Socket {
        sptr(Pair);

        Pair(Context& context);
        Pair(Pair&& other);
        Pair&operator=(Pair&& other);

        Pair(const Pair& other) = delete;
        Pair&operator=(const Pair& other) = delete;
    };
}
#endif //SUIL_ZMQ_H
