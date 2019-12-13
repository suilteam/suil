//
// Created by dc on 2019-12-12.
//

#include "zmq.h"

namespace suil::zmq {

    Context::Context()
        : handle{zmq_ctx_new()}
    {}

    Context::Context(Context &&other)
        : handle{other.handle}
    {
        other.handle = nullptr;
    }

    Context& Context::operator=(Context &&other) {
        Ego.handle = other.handle;
        other.handle = nullptr;
        return Ego;
    }

    Context::~Context() {
        if (Ego.handle) {
            zmq_ctx_destroy(Ego.handle);
        }
    }

    Message::Message()
    {
        if (zmq_msg_init(&msg)) {
            throw Exception::create("failed to create zmq message: ", zmq_strerror(zmq_errno()));
        }
        initialized = true;
    }

    Message::Message(size_t size)
    {
        if (zmq_msg_init_size(&msg, size)) {
            throw Exception::create("failed to create zmq message of size ",
                    size, " : ", zmq_strerror(zmq_errno()));
        }
        initialized = true;
    }

    Message::Message(void *data, size_t size, bool own)
    {
        if (zmq_msg_init_data(&msg, data, size, (own? destroy: nullptr), nullptr)) {
            throw Exception::create("failed to initilize zmq message with buffer of size ", size,
                    " : ", zmq_strerror(zmq_errno()));
        }
        initialized = true;
    }

    Message::Message(suil::zmq::Message &&other)
        : msg{other.msg},
          initialized{other.initialized}
    {
        memset(&other.msg, 0, sizeof(msg));
        other.initialized = false;
    }

    Message& Message::operator=(suil::zmq::Message &&other) {
        Ego.msg = other.msg;
        Ego.initialized = false;
        memset(&other.msg, 0, sizeof(msg));
        other.initialized = false;
        return Ego;
    }

    Message::~Message() {
        if (Ego.initialized) {
            zmq_msg_close(&Ego.msg);
            Ego.initialized = false;
        }
    }

    void* Message::data() {
        if (Ego.initialized)
            return zmq_msg_data(&Ego.msg);
        return nullptr;
    }

    size_t Message::size() const {
        if (Ego.initialized)
            return zmq_msg_size(&Ego.msg);
        return 0;
    }

    void Message::destroy(void *data, void *hint)
    {
        if (data) {
            ::free(data);
        }
    }


    Socket::Socket(Context &context, void *sock)
        : ctx{context},
          sock{sock}
    {
        resolveSocket();
    }

    Socket::Socket(suil::zmq::Socket &&other)
        : sock{sock},
          ctx{other.ctx}
    {
        other.sock = nullptr;
        other.fd = -1;
    }

    Socket& Socket::operator=(suil::zmq::Socket &&other) {
        Ego.sock = other.sock;
        Ego.fd = other.fd;
        other.sock = nullptr;
        other.fd = -1;
        return Ego;
    }

    void Socket::resolveSocket()
    {
        if (sock == nullptr) {
            throw Exception::create("Cannot create a socket with a null zmq socket");
        }
        size_t sz{sizeof(Ego.fd)};
        if (zmq_getsockopt(sock, ZMQ_FD, &Ego.fd, &sz)) {
            throw Exception::create("Cannot access zmq socket descriptor: ", errno_s);
        }
        idebug("%p has  zmq socket %d", this, Ego.fd);
    }

    Message Socket::receive(int64_t to)
    {
        if (sock == nullptr) {
            throw Exception::create("cannot receive from a non-existent zmq socket");
        }

        Message msg;
        auto dd = to < 0? -1: mnow() + to;
        do {
            if (!zmq_msg_recv(msg, Ego.sock, ZMQ_NOBLOCK)) {
                if (zmq_errno() == EAGAIN) {
                    // wait for socket to be writable
                    int ev = fdwait(Ego.fd, FDW_OUT, dd);
                    if (ev & FDW_ERR) {
                        // error while waiting for file to be writable
                        ierror("error while wait for zmq socket (%d) to writable: %s", Ego.fd, errno_s);
                        return {};
                    }
                    continue;
                }
                ierror("zmq_msg_recv(%d) error: %s", Ego.fd, zmq_strerror(zmq_errno()));
                return {};
            }
            else {
                idebug("received zmq msg {fd:%d, size:%zu}", Ego.fd, msg.size());
                break;
            }
        } while (true);

        return msg;
    }

    bool Socket::send(const Message &msg, int64_t to)
    {
        if (sock == nullptr) {
            throw Exception::create("cannot send to a non-existent zmq socket");
        }

        do {
            if (zmq_msg_send(msg, Ego.sock, ZMQ_NOBLOCK)) {
                if (zmq_errno() == EAGAIN) {
                    int ev = fdwait(Ego.fd, FDW_OUT, to);
                    if (ev&FDW_ERR) {
                        // error while waiting for file to be writable
                        ierror("error while wait for zmq socket (%d) to readable: %s", Ego.fd, errno_s);
                        return {};
                    }
                    continue;
                }
            }
            else {
                idebug("sent %d bytes to zmq socket (%d)", msg.size(), Ego.fd);
                break;
            }
        } while (true);

        return true;
    }

    Socket::~Socket() {
        if (sock) {
            zmq_close(sock);
            sock = nullptr;
            fd = -1;
        }
    }
}