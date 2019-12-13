//
// Created by dc on 2019-12-12.
//

#ifndef SUIL_ZMQ_H
#define SUIL_ZMQ_H

#include <suil/logging.h>
#include <suil/buffer.h>
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
        Message(size_t size);
        Message(void *data, size_t size, bool own = false);

        Message(Message&& other);
        Message&operator=(Message&& other);

        Message(const Message&) = delete;
        Message&operator=(const Message&) = delete;

        void*  data();
        size_t size() const;

        inline bool empty() const {
            return Ego.size() == 0;
        }

        operator suil::Data() {
            return suil::Data{Ego.data(), Ego.size(), false};
        }

        inline operator zmq_msg_t*() const {
            return const_cast<zmq_msg_t*>(&msg);
        }

        inline operator bool() const {
            return Ego.empty();
        }

        ~Message();

    private:
        static void destroy(void *data, void *hint);
        zmq_msg_t msg;
        bool      initialized{false};
    };

    struct Socket : LOGGER(ZMQ) {

        Socket(Context& context, void* sock);
        Socket(Socket&& other);
        Socket&operator=(Socket&& other);

        Socket(const Socket&) = delete;
        Socket&operator=(const Socket&) = delete;

        Message receive(int64_t to = -1);

        bool send(const Message& msg, int64_t to = -1);

        inline bool send(const void* buf, size_t sz, int64_t to = -1) {
            Message msg(const_cast<void*>(buf), sz);
            return send(msg, to);
        }

        inline bool send(const suil::Data& data, int64_t to = -1) {
            return send(data.cdata(), data.size());
        }

        inline bool send(const suil::OBuffer& data, int64_t to = -1) {
            return send(data.data(), data.size(), to);
        }

        inline bool send(const suil::String& data, int64_t to = -1) {
            return send(data.data(), data.size(), to);
        }

        virtual ~Socket();

    protected:
        void resolveSocket();
        void* sock{nullptr};
    private:
        int  fd{-1};
        Context& ctx;
    };
}
#endif //SUIL_ZMQ_H
