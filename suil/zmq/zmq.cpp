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


    Socket::Socket(Context &context, int type)
        : ctx{context},
          sock{zmq_socket(context, type)}
    {
        if (sock == nullptr) {
            throw Exception::create("failed to create a new zmq socket: ", zmq_strerror(zmq_errno()));
        }
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

    bool Socket::resolveSocket()
    {
        size_t sz{sizeof(Ego.fd)};
        if (zmq_getsockopt(sock, ZMQ_FD, &Ego.fd, &sz)) {
            ierror("Cannot access zmq socket descriptor: %s", zmq_strerror(zmq_errno()));
            return false;
        }
        idebug("%p has  zmq socket %d", this, Ego.fd);
        return true;
    }

    Message Socket::receive(int64_t to)
    {
        if (sock == nullptr) {
            throw Exception::create("cannot receive from a non-existent zmq socket");
        }

        Message msg;
        auto dd = to < 0? -1: mnow() + to;
        do {
            if (zmq_msg_recv(msg, Ego.sock, ZMQ_DONTWAIT) == -1) {
                if (zmq_errno() == EAGAIN) {
                    int ev = fdwait(Ego.fd, FDW_OUT, dd);
                    if (ev & FDW_ERR) {
                        ierror("error while wait for zmq socket (%d) to readable: %s", Ego.fd, errno_s);
                        return {};
                    }
                    continue;
                    msg = Message{};
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

        auto dd = to < 0? -1: mnow() + to;
        do {
            if (zmq_msg_send(msg, Ego.sock, ZMQ_DONTWAIT) == -1) {
                if (zmq_errno() == EAGAIN) {
                    int ev = fdwait(Ego.fd, FDW_OUT, dd);
                    if (ev&FDW_ERR) {
                        // error while waiting for file to be writable
                        ierror("error while wait for zmq socket (%d) to readable: %s", Ego.fd, errno_s);
                        return {};
                    }
                    continue;
                }
                ierror("zmq_msg_send(%d) error: %s", Ego.fd, zmq_strerror(zmq_errno()));
                return {};
            }
            else {
                idebug("sent %d bytes to zmq socket (%d)", msg.size(), Ego.fd);
                break;
            }
        } while (true);

        return true;
    }

    void Socket::close() {
        if (sock) {
            zmq_close(sock);
            sock = nullptr;
            fd = -1;
        }
    }

    Socket::~Socket() {
        Ego.close();
    }

    Requestor::Requestor(Context& context)
        : Socket(context, ZMQ_REQ)
    {}

    Requestor::Requestor(Requestor&& other)
        : Socket(std::move(other))
    {}

    Requestor& Requestor::operator=(Requestor&& other)
    {
        Socket::operator=(std::move(other));
        return Ego;
    }

    bool Requestor::connect(const char* endpoint)
    {
        if (Ego.sock == nullptr) {
            Ego.sock = zmq_socket(Ego.ctx, ZMQ_REQ);
            if (Ego.sock == nullptr) {
                throw Exception::create("failed to create a new ZMQ_REQ socket: ", zmq_strerror(zmq_errno()));
            }
        }

        if (zmq_connect(Ego.sock, endpoint)) {
            ierror("failed to connect to zmq endpoint '%s': %s", endpoint, zmq_strerror(zmq_errno()));
            return false;
        }
        idebug("Connected to zmq endpoint %s", endpoint);
        return resolveSocket();
    }

    Responder::Responder(Context& context)
        : Socket(context, ZMQ_REP)
    {}

    Responder::Responder(Responder&& other)
        : Socket(std::move(other))
    {}

    Responder& Responder::operator=(Responder&& other)
    {
        Socket::operator=(std::move(other));
        return Ego;
    }

    bool Responder::bind(const char* endpoint)
    {
        if (Ego.sock == nullptr) {
            Ego.sock = zmq_socket(Ego.ctx, ZMQ_REP);
            if (Ego.sock == nullptr) {
                throw Exception::create("failed to create a new ZMQ_REP socket: ", zmq_strerror(zmq_errno()));
            }
        }

        if (zmq_bind(Ego.sock, endpoint)) {
            ierror("failed to bind to zmq endpoint '%s': %s", endpoint, zmq_strerror(zmq_errno()));
            return false;
        }
        idebug("Bound to zmq endpoint %s", endpoint);
        return resolveSocket();
    }
}