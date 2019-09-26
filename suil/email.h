//
// Created by dc on 11/16/17.
//

#ifndef SUIL_SMTP_HPP
#define SUIL_SMTP_HPP

#include <list>

#include <suil/sock.h>
#include <suil/base64.h>
#include "channel.h"

namespace suil {

    define_log_tag(SMTP_CLIENT);

    template <typename Proto>
    struct Stmp;

    namespace __internal {
        struct client;
    }

    struct Email {
        using unique_ptr = std::unique_ptr<Email>;

        struct Address {
            Address(const String& email, const String& name = nullptr)
                : name(String(name).dup()),
                  email(String(email).dup())
            {}
            String name;
            String email;

            inline void encode(OBuffer& b) const {
                if (!name.empty()) {
                    b << "'" << name << "'";
                }
                b << "<" << email << ">";
            }
        };

        struct Attachment {
            String   fname;
            String   mimetype;
            char    *data{nullptr};
            off_t    size{0};
            size_t   len{0};
            bool     mapped{false};

            Attachment(const String& fname)
                : fname(fname.dup()),
                  mimetype(String(utils::mimetype(fname.peek())).dup())
            {}

            Attachment(const Attachment&) = delete;
            Attachment&operator=(const Attachment&) = delete;

            Attachment(Attachment&& other)
                : fname(std::move(other.fname)),
                  mimetype(std::move(other.mimetype)),
                  data(other.data),
                  size(other.size),
                  len(other.len),
                  mapped(other.mapped)
            {
                other.data = nullptr;
                other.mapped = false;
                size = len = 0;
            }



            Attachment&operator=(Attachment&& other)
            {
                fname = std::move(other.fname);
                mimetype = std::move(other.mimetype);
                data = other.data;
                size = other.size;
                len  = other.len;
                mapped = other.mapped;

                other.data = nullptr;
                other.size = other.len = 0;
                other.mapped = false;
                return Ego;
            }

            inline bool load();

            void send(SocketAdaptor& sock, const String& boundary, int64_t timeout);

            inline const char *filename() const {
                const char *tmp = strrchr(fname.data(), '/');
                if (tmp) return tmp+1;
                return fname.data();
            }

            ~Attachment();
        };

        Email(Address addr, const String subject)
            : subject(subject.dup())
        {
            to(std::move(addr));
            String  tmp = utils::catstr("suil", utils::randbytes(4));
            boundry = utils::base64::encode(tmp);
        }

        template <typename... __T>
        inline void to(Address addr, __T... other) {
            pto(std::move(addr));
            to(other...);
        }

        template <typename... __T>
        inline void cc(Address addr, __T... other) {
            pcc(std::move(addr));
            cc(other...);
        }

        template <typename... __T>
        inline void bcc(Address addr, __T... other) {
            pbcc(std::move(addr));
            bcc(other...);
        }

        template <typename... __T>
        inline void attach(String fname, __T... other) {
            pattach(fname);
            attach(other...);
        }

        inline void content(const char *type) {
            body_type = String(type).dup();
        }

        inline void body(const char *msg, const char *type = nullptr) {
            String tmp(msg);
            body(tmp, type);
        }

        inline void body(String& msg, const char *type = nullptr) {
            if (type != nullptr)
                content(type);
            bodybuf << msg;
        }

        inline OBuffer& body() {
            return bodybuf;
        }

    private:
        String gethead(const Address &from);

        template <typename Proto>
        friend struct suil::Stmp;
        friend struct suil::__internal::client;

        inline void to(){}
        inline void pto(Address&& addr) {
            if (!exists(addr)) {
                receipts.emplace_back(addr);
            }
        }

        inline void cc(){}
        inline void pcc(Address&& addr) {
            if (!exists(addr)) {
                ccs.emplace_back(addr);
            }
        }

        inline void bcc(){}
        inline void pbcc(Address&& addr) {
            if (!exists(addr)) {
                bccs.emplace_back(addr);
            }
        }

        inline void attach(){}
        inline void pattach(const String& fname) {
            if (attachments.end() ==  attachments.find(fname)) {
                attachments.emplace(fname.dup(), Attachment(fname));
            }
        }

        inline bool exists(const Address& addr) {
            auto predicate = [&](Address& in) {
                return in.email == addr.email;
            };
            return (receipts.end() != std::find_if(receipts.begin(), receipts.end(), predicate)) &&
                   (ccs.end() != std::find_if(ccs.begin(), ccs.end(), predicate)) &&
                   (bccs.end() != std::find_if(bccs.begin(), bccs.end(), predicate));
        }

        String                  body_type{"text/plain"};
        String                  subject;
        String                  boundry;
        std::vector<Address>    receipts;
        std::vector<Address>    ccs;
        std::vector<Address>    bccs;
        CaseMap<Attachment>     attachments;
        OBuffer                 bodybuf;
    };

    namespace __internal {

        struct client : LOGGER(SMTP_CLIENT) {
            client(SocketAdaptor& sock)
                :sock(sock)
            {}

        private:
            template <typename Proto>
            friend struct suil::Stmp;

            bool sendaddresses(const std::vector<Email::Address>& addrs, int64_t timeout);
            bool sendhead(Email& msg, int64_t timeout);
            void send(const Email::Address& from, Email& msg, int64_t timeout);

            bool login(const String& domain, const String& username, const String& passwd);

            int getresponse(int64_t timeout);

            void quit();

            const char* showerror(int code);

            inline bool sendflush(int64_t timeout) {
                if (sock.send("\r\n", 2, timeout) == 2)
                    return sock.flush(timeout);
                return false;
            }

            inline bool sendline(int64_t timeout) { return  true; }

            inline bool send_data(int64_t timeout, const char *cmd) {
                size_t len = strlen(cmd);
                return sock.send(cmd, len, timeout) == len;
            }

            inline bool send_data(int64_t timeout, const String& str) {
                return sock.send(str.data(), str.size(), timeout) == str.size();
            }

            template <typename... P>
            inline ssize_t sendline(int64_t timeout, const String data, P... p) {
                if (send_data(timeout, data) && sendline(timeout, std::forward<P>(p)...)) {
                    return sendflush(timeout);
                }
                return false;
            }

            ~client();

            SocketAdaptor&    sock;
        };
    }

    template <typename Proto = SslSock>
    struct Stmp : LOGGER(SMTP_CLIENT) {
        /**
         * @brief Creates a new smtp mail server
         * @param server the server address
         * @param port the stmp port on the server
         */
        Stmp(const char *server, int port)
            : server(String(server).dup()),
              port(port),
              sender(proto)
        {}

        Stmp()
            : server{nullptr},
              port(0),
              sender(proto)
        {}

        template <typename ...Params>
        inline void send(Email& msg, Email::Address from, Params... params) {
            if (!proto.isopen()) {
                // cannot send without logging
                throw Exception::create("Send email failed: not connected to server");
            }
            auto opts = iod::D(params...);
            do_send(msg, from, opts);
        }

        template <typename... Params>
        inline bool login(const String username, const String passwd, Params... params) {
            if (proto.isopen()) {
                throw Exception::create("Already logged into mail-server");
            }
            auto opts = iod::D(params...);
            return do_login(username, passwd, opts);
        }


        inline void setup(String&& server, int port) {
            Ego.server = std::move(server);
            Ego.port   = port;
        }

    private:

        template <typename __P>
        void do_send(Email& msg, Email::Address& from, __P& params) {
            int64_t timeout = params.get(sym(timeout), 1500);
            sender.send(from, msg, timeout);
        }
        template <typename __P>
        bool do_login(const String& user, const String& passwd, __P& params) {
            int64_t timeout = params.get(sym(timeout), 1500);
            String domain = params.get(sym(domain),  "localhost");

            ipaddr addr = ipremote(server.data(), port, 0, utils::after(timeout));
            if (errno != 0) {
                ierror("server address '%s:%d' could not be resolved: %s",
                            server.data(), port, errno_s);
                return false;
            }

            // open Connection to server using underlying protocol (either raw TCP or SSL)
            if (!proto.connect(addr, timeout)) {
                ierror("connecting to server '%s:%d' failed: %s",
                          server.data(), port, errno_s);
                return false;
            }
            ierror("Connection to email server '%s:%d' open", server.data(), port);

            // login to server
            return sender.login(domain, user, passwd);
        }

        Proto proto{};
        __internal::client sender;
        String    server;
        int       port;
    };

    template <typename Sock>
    struct MailOutbox : LOGGER(SMTP_CLIENT) {
        using unique_ptr = std::unique_ptr<MailOutbox<Sock>>;
    private:

        using Sync = Channel<char*>;

        struct Composed {
            using shared_ptr = std::shared_ptr<Composed>;
            Composed(Email::unique_ptr&& mail, int64_t timeout)
                : expire(utils::after(timeout)),
                  email(std::move(mail))
            {}

            Composed(Composed&& composed)
                : expire(composed.expire),
                  sync(std::move(composed.sync)),
                  email(std::move(composed.email))
            { }

            Composed& operator=(Composed&& composed) {
                expire = composed.expire;
                sync = std::move(composed.sync);
                email = std::move(composed.email);
                return Ego;
            }

            Composed(const Composed&) = delete;
            Composed& operator=(const Composed&) = delete;

            inline int64_t getExpire() const {
                return Ego.expire;
            }

            inline Sync& getSync() {
                return Ego.sync;
            }

            inline Email& getEmail() {
                return *email;
            }

            inline bool done(bool set = false) {
                if (!isDone) {
                    if (set) isDone = true;
                    return false;
                }
                return true;
            }

        private:
            int64_t             expire;
            Sync                sync{(char *)0x0A};
            Email::unique_ptr   email;
            bool                isDone{false};
        };

    public:

        MailOutbox(const String& server, int port, Email::Address sender)
            : stmp(server(), port),
              sender{sender}
        {}

        MailOutbox(const MailOutbox&) = delete;
        MailOutbox& operator=(const MailOutbox&) = delete;
        MailOutbox(MailOutbox&&) = delete;
        MailOutbox& operator=(MailOutbox&&) = delete;

        template <typename... Params>
        inline bool login(const String username, const String passwd, Params... params) {
            return Ego.stmp.login(username, passwd, std::forward<Params>(params)...);
        }

        inline Email::unique_ptr draft(const String& to, const String& subject) {
            return std::make_unique<Email>(Email::Address{to()}, subject);
        }

        String send(Email::unique_ptr&& mail, int64_t timeout = -1) {
            auto ob = std::make_shared<Composed>(std::move(mail), timeout);
            sendQ.push_back(ob);
            if (timeout > 0) {
                if (!Ego.sending) {
                    go(sendOutbox(Ego));
                }

                char *msg{nullptr};
                if ((ob->getSync()[timeout] >> msg)) {
                    // it will be erased by sending coroutine
                    return String{msg, (msg? strlen(msg): 0), (msg != nullptr)};
                }
                else {
                    return String{"Sending email message timed out"};
                }
            }
            else {
                if (!Ego.sending) {
                    go(sendOutbox(Ego));
                }
                return String{};
            }
        }
    private:

        static coroutine void sendOutbox(MailOutbox<Sock>& self) {
            self.sending = true;
            while (!self.sendQ.empty() && !self.quiting) {
                auto& ob = self.sendQ.back();
                char *status{nullptr};
                try {
                    self.stmp.send(ob->getEmail(), self.sender);
                }
                catch (...) {
                    status = strdup(Exception::fromCurrent().what());
                }

                if (mnow() < ob->getExpire()) {
                    // job was done, notify waiting sender
                    ob->getSync() << status;
                }
                else {
                    if (status != nullptr) {
                        lerror(&self, "sending mail failed: %s", status);
                        free(status);
                    }
                }
                self.sendQ.pop_back();
                yield();
            }
            self.sending = false;
        }

        using SendQueue = std::list<typename Composed::shared_ptr>;
        SendQueue             sendQ;
        Stmp<Sock>            stmp;
        Email::Address        sender;
        bool                  quiting{false};
        bool                  sending{false};
    };

    using TcpMailOutbox = MailOutbox<TcpSock>;
    using SslMailOutbox = MailOutbox<SslSock>;

}
#endif //SUIL_SMTP_HPP
