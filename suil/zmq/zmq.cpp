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

    Message::Message(suil::zmq::Message &&other)
        : msg{other.msg},
          initialized{other.initialized}
    {
        //memset(&other.msg, 0, sizeof(msg));
        other.initialized = false;
    }

    Message& Message::operator=(suil::zmq::Message &&other) {
        Ego.msg = other.msg;
        Ego.initialized = false;
        //memset(&other.msg, 0, sizeof(msg));
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

    const void* Message::cdata() const {
        if (Ego.initialized)
            return zmq_msg_data(const_cast<zmq_msg_t *>(&Ego.msg));
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
            sdebug("DESTROYING");
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
        id = utils::randbytes(4);
        zmq_setsockopt(Ego.sock, ZMQ_IDENTITY, Ego.id(), Ego.id.size());
    }

    Socket::Socket(suil::zmq::Socket &&other)
        : sock{sock},
          ctx{other.ctx},
          id{std::move(other.id)}
    {
        other.sock = nullptr;
        other.fd = -1;
    }

    Socket& Socket::operator=(suil::zmq::Socket &&other) {
        Ego.sock = other.sock;
        Ego.fd = other.fd;
        Ego.id = std::move(other.id);
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
        idebug("%s has  zmq socket %d", Ego.id(), Ego.fd);
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
                    int ev = fdwait(Ego.fd, FDW_IN, dd);
                    if (ev & FDW_ERR) {
                        ierror("error while wait for zmq socket (%s) to readable: %s", Ego.id(), errno_s);
                        return {};
                    }
                    continue;
                }
                ierror("zmq_msg_recv(%s) error: %s", Ego.id(), zmq_strerror(zmq_errno()));
                return {};
            }
            else {
                idebug("received zmq msg {id:%s, size:%zu}", Ego.id(), msg.size());
                break;
            }
        } while (true);

        return msg;
    }

    bool Socket::send(const void* buf, size_t sz, int64_t to)
    {
        if (sock == nullptr) {
            throw Exception::create("cannot send to a non-existent zmq socket");
        }

        auto dd = to < 0? -1: mnow() + to;
        do {
            auto sent = zmq_send(Ego.sock, buf, sz, ZMQ_DONTWAIT);
            if (sent == -1) {
                if (zmq_errno() == EAGAIN) {
                    int ev = fdwait(Ego.fd, FDW_OUT, dd);
                    if (ev&FDW_ERR) {
                        // error while waiting for file to be writable
                        ierror("error while wait for zmq socket (%s) to readable: %s", Ego.id(), errno_s);
                        return {};
                    }
                    continue;
                }
                ierror("zmq_send(%s) error: %s", Ego.id(), zmq_strerror(zmq_errno()));
                return {};
            }
            else {
                idebug("sent %d bytes to zmq socket (%s)", sent, Ego.id());
                break;
            }
        } while (true);

        return true;
    }

    bool Socket::send(Message& msg, int64_t to)
    {
        if (sock == nullptr) {
            throw Exception::create("cannot send to a non-existent zmq socket");
        }

        auto dd = to < 0? -1: mnow() + to;
        do {
            auto sent = zmq_msg_send(msg, Ego.sock, ZMQ_DONTWAIT);
            if (sent == -1) {
                if (zmq_errno() == EAGAIN) {
                    int ev = fdwait(Ego.fd, FDW_OUT, dd);
                    if (ev&FDW_ERR) {
                        // error while waiting for file to be writable
                        ierror("error while wait for zmq socket (%s) to readable: %s", Ego.id(), errno_s);
                        return {};
                    }
                    continue;
                }
                ierror("zmq_msg_send(%s) error: %s", Ego.id(), zmq_strerror(zmq_errno()));
                return {};
            }
            else {
                idebug("sent %d bytes to zmq socket (%s)", sent, Ego.id());
                break;
            }
        } while (true);

        return true;
    }

    bool Socket::monitor(const suil::String& endpoint, int events) {
        if (!Ego.sock) {
            throw Exception::create("Cannot monitor a null socket");
        }
        return zmq_socket_monitor(Ego.sock, endpoint.c_str(), events) == 0;
    }

    void Socket::close() {
        if (sock) {
            idebug("Closing socket %s", Ego.id());
            zmq_close(sock);
            sock = nullptr;
            fd = -1;
        }
    }

    Socket::~Socket() {
        Ego.close();
    }

    Requestor::Requestor(Context& context, int type)
        : Socket(context, type)
    {}

    Requestor::Requestor(Requestor&& other)
        : Socket(std::move(other))
    {}

    Requestor& Requestor::operator=(Requestor&& other)
    {
        Socket::operator=(std::move(other));
        return Ego;
    }

    bool Socket::isConnected() const {
        return Ego.fd != -1;
    }

    bool Socket::connect(const suil::String& endpoint)
    {
        if (Ego.sock == nullptr) {
            Ego.sock = zmq_socket(Ego.ctx, ZMQ_REQ);
            if (Ego.sock == nullptr) {
                throw Exception::create("failed to create a new ZMQ_REQ socket: ", zmq_strerror(zmq_errno()));
            }
        }

        if (zmq_connect(Ego.sock, endpoint.c_str())) {
            ierror("failed to connect to zmq endpoint '%s': %s", endpoint(), zmq_strerror(zmq_errno()));
            return false;
        }
        idebug("Connected to zmq endpoint %s", endpoint());
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

    bool Socket::bind(const suil::String& endpoint)
    {
        if (Ego.sock == nullptr) {
            Ego.sock = zmq_socket(Ego.ctx, ZMQ_REP);
            if (Ego.sock == nullptr) {
                throw Exception::create("failed to create a new ZMQ_REP socket: ", zmq_strerror(zmq_errno()));
            }
        }

        if (zmq_bind(Ego.sock, endpoint.c_str())) {
            ierror("failed to bind to zmq endpoint '%s': %s", endpoint, zmq_strerror(zmq_errno()));
            return false;
        }
        idebug("Bound to zmq endpoint %s", endpoint());
        return resolveSocket();
    }

    Dealer::Dealer(Context& context)
        : Socket(context, ZMQ_DEALER)
    {}

    Dealer::Dealer(Dealer&& other)
        : Socket(std::move(other))
    {}

    Dealer& Dealer::operator=(Dealer&& other) {
        Socket::operator=(std::move(other));
        return Ego;
    }

    Pair::Pair(Context& context)
        : Socket(context, ZMQ_PAIR)
    {}

    Pair::Pair(Pair&& other)
        : Socket(std::move(other))
    {}

    Pair& Pair::operator=(Pair&& other) {
        Socket::operator=(std::move(other));
        return Ego;
    }
}