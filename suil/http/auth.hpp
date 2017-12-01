//
// Created by dc on 8/10/17.
//

#ifndef SUIL_AUTH_HPP
#define SUIL_AUTH_HPP

#include <suil/http.hpp>
#include <suil/sql/sqlite.hpp>

namespace suil {
    namespace http {

        define_log_tag(AUTHENTICATION);
        struct jwt_t {
            typedef decltype(iod::D(
                prop(typ, zcstring),
                prop(alg, zcstring)
            )) jwt_header_t;

            typedef decltype(iod::D(
                s::_iss(var(optional), var(ignore))    = zcstring(),
                s::_aud(var(optional), var(ignore))    = zcstring(),
                s::_sub(var(optional), var(ignore))    = zcstring(),
                s::_exp(var(optional), var(ignore))    = int64_t(),
                s::_roles(var(optional), var(ignore)) = std::vector<zcstring>(),
                s::_claims(var(optional)) = iod::json_string()
            )) jwt_payload_t;

            jwt_t(const char* typ)
            {
                header.typ = zcstring(typ).dup();
                header.alg = zcstring("HS256").dup();
            }

            jwt_t()
                : jwt_t("JWT")
            {}

            template <typename __T>
            inline void claims(__T& claims) {
                /* encode the claims to a json string*/;
                payload.claims = std::move(iod::json_encode(claims));
            }

            template <typename... __Claims>
            inline void claims(__Claims... c) {
                auto tmp = iod::D(c...);
                /* encode the claims to a json string*/
                claims(tmp);
            }

            inline void roles(std::vector<zcstring>& roles) {
                for (auto& r : roles) {
                    payload.roles.push_back(std::move(r.dup()));
                }
            }

            inline std::vector<zcstring>& roles() {
                return payload.roles;
            }

            inline void roles(const char *r) {
                payload.roles.push_back(std::move(zcstring(r).dup()));
            }

            template <typename... __T>
            inline void roles(const char *a, __T... args) {
                roles(a);
                roles(args...);
            }

            template <typename __T>
            inline __T claims() {
                __T c;
                iod::json_decode(c, payload.claims);
                return std::move(c);
            }

            inline const zcstring& typ() const {
                return header.typ;
            }

            inline const zcstring& alg() const {
                return header.alg;
            }

            inline const zcstring& iss() const {
                return payload.iss;
            }

            inline void iss(zcstring& iss){
                payload.iss = iss.dup();
            }

            inline void iss(const char* iss){
                payload.iss = zcstring(iss).dup();
            }

            inline const zcstring& aud() const {
                return payload.aud;
            }

            inline void aud(zcstring& aud) {
                payload.aud = aud.dup();
            }

            inline void aud(const char *aud) {
                payload.aud = zcstring(aud).dup();
            }

            inline const zcstring sub() const {
                return payload.sub;
            }

            inline void sub(zcstring& sub) {
                payload.sub = sub.dup();
            }

            inline void sub(const char* sub) {
                payload.sub = zcstring(sub).dup();
            }

            inline const int64_t exp() const {
                return payload.exp;
            }

            inline void exp(int64_t exp) {
                payload.exp = exp;
            }

            static bool decode(jwt_t& jwt, zcstring&& zcstr, zcstring& secret);
            static bool verify(zcstring&& zcstr, zcstring& secret);
            zcstring encode(zcstring& secret);

        private:
            jwt_header_t  header;
            jwt_payload_t payload;
        };

        typedef decltype(iod::D(
            s::_id(var(AUTO_INCREMENT), var(UNIQUE), var(optional)) = int(),
            s::_username(var(PRIMARY_KEY)) = zcstring(),
            s::_email(var(UNIQUE))         = zcstring(),
            prop(passwd,     zcstring),
            s::_salt(var(optional))        = zcstring(),
            prop(fullname,   zcstring),
            s::_roles(var(optional)) = std::vector<zcstring>(),
            s::_verified(var(optional)) = bool(),
            s::_locked(var(optional))   = bool()
        )) user_t;

        struct rand_8byte_salt {
            zcstring operator()(const zcstring & /*username*/);
            zcstring operator()(const char */* username*/) {
                zcstring tmp;
                return (*this)(tmp);
            }
        };

        struct pbkdf2_sha1_hash {
            pbkdf2_sha1_hash(const zcstring& appsalt)
                : appsalt(appsalt)
            {}

            zcstring operator()(zcstring& passwd, zcstring& salt);

            zcstring operator()(const char *passwd, const char* salt) {
                zcstring tpass(passwd);
                zcstring tsalt(salt);

                return (*this)(tpass, tsalt);
            }

        private:
            const zcstring& appsalt;
        };

        struct JwtUse {
            typedef enum { HEADER, COOKIE} From;
            JwtUse()
                : use(From::HEADER)
            {}
            JwtUse(From from, const char *key)
                : use(from),
                  key(zcstring(key).dup())
            {}

            From use;
            zcstring key{"Authorization"};
        };
        struct jwt_authorization : LOGGER(dtag(AUTHENTICATION)) {
            struct Context{
                Context()
                    : send_tok(0),
                      encode(0),
                      request_auth(0)
                {}

                inline void authorize(jwt_t&& jwt, user_t& user) {
                    this->jwt = std::move(jwt);
                    this->jwt.roles(user.roles);
                    this->jwt.sub(user.email);
                    send_tok = 1;
                    encode = 1;
                    request_auth = 0;
                }

                inline Status authenticate(Status status, const char *msg = NULL) {
                    send_tok = 0;
                    encode   = 0;
                    request_auth = 1;
                    if (msg) {
                        token_hdr = zcstring(msg).dup();
                    }

                    return status;
                }

                inline Status authenticate(const char *msg = NULL) {
                    return authenticate(Status::UNAUTHORIZED, msg);
                }

                inline void logout(const char* redirect = NULL) {
                    if (redirect) {
                        logout_url = zcstring(redirect).dup();
                    }
                    // do not send token, but send delete cookie if using cookie method
                    send_tok = 0;
                }

                const zcstring& token() const  {
                    return actual_token;
                }

            private:
                jwt_t    jwt;
                friend struct jwt_authorization;
                union {
                    struct {
                        uint8_t send_tok : 1;
                        uint8_t encode : 1;
                        uint8_t request_auth: 1;
                        uint8_t __u8r5: 5;
                    };
                    uint8_t     _u8;
                } __attribute__((packed));
                zcstring token_hdr{};
                zcstring actual_token{};
                zcstring logout_url{};
            };

            void before(request& req, response& resp, Context& ctx) {
                if (req.route().AUTHORIZE) {
                    zcstring tkhdr;
                    if (use.use == JwtUse::HEADER) {
                        auto auth = req.header(use.key);
                        if (auth.empty() ||
                            strncasecmp(auth.data(), "Bearer ", 7)) {
                            /* user is not authorized */
                            authrequest(resp);
                        }
                        /* temporary dup of the token */
                        tkhdr = zcstring(auth.data(), auth.size(), false);
                        ctx.actual_token = zcstring(&auth.data()[7], auth.size() - 7, false);
                    }
                    else {
                        // token
                        cookie_it cookies = req();
                        auto auth = cookies[use.key];
                        if (!auth) {
                            /* user is not authorized */
                            authrequest(resp);
                        }
                        tkhdr = auth;
                        ctx.actual_token = auth;
                    }

                    if (ctx.actual_token.len == 0) {
                        /* no need to proceed, token is invalid */
                        authrequest(resp);
                    }

                    zcstring token(ctx.actual_token.dup());

                    bool valid{false};
                    try {
                        valid = jwt_t::decode(ctx.jwt, std::move(token), key);
                    }
                    catch(...) {
                        /* error decoding token */
                        authrequest(resp, "Invalid authorization token.");
                    }

                    if (!valid || (ctx.jwt.exp() < time(NULL))) {
                        /* token unauthorized */
                        authrequest(resp);
                    }

                    if (!req.route().AUTHORIZE.check(ctx.jwt.roles())) {
                        /* token does not have permission to access resouce */
                        authrequest(resp, "Access to resource denied.");
                    }

                    ctx.token_hdr = tkhdr;
                    ctx.send_tok = 1;
                }
            }

            void after(request&, http::response& resp, Context& ctx) {
                /* if authorized token should have been set */
                if (ctx.send_tok) {
                    zcstring tok{nullptr}, tok2{nullptr};
                    /* send token in header */
                    if (ctx.encode) {
                        /* encode the current json web-token*/
                        ctx.jwt.exp(time(NULL) + expiry);
                        zcstring encoded(ctx.jwt.encode(key));
                        tok = utils::catstr("Bearer ", encoded);
                        tok2 = std::move(encoded);
                    }
                    else {
                        /* move header from request */
                        tok = std::move(ctx.token_hdr);
                    }

                    if (use.use == JwtUse::HEADER) {
                        resp.header(use.key.peek(), std::move(tok));
                    }
                    else {
                        cookie_t cookie(use.key.cstr);
                        cookie.domain(domain.peek());
                        cookie.path(path.peek());
                        if (tok2) {
                            cookie.value(std::move(tok2));
                        } else {
                            cookie.value(std::move(tok));
                        }
                        cookie.expires(time(NULL) + expiry);
                        resp.cookie(cookie);
                    }
                } else if(!ctx.logout_url.empty()) {
                    resp.redirect(Status::FOUND, ctx.logout_url.cstr);
                    if (use.use == JwtUse::COOKIE) {
                        // delete cookie
                        delete_cookie(resp);
                    }
                } else if (ctx.request_auth) {
                    /* send authentication request */
                    resp.header("WWW-Authenticate", authenticate);
                    if (ctx.token_hdr) {
                        throw error::unauthorized(ctx.token_hdr.cstr);
                    }
                    else {
                        throw error::unauthorized();
                    }
                }
            }

            template <typename __Opts>
            void configure(__Opts& opts) {
                /* configure expiry time */
                expiry = opts.get(sym(expires), 3600);
                zcstring tmp(opts.get(sym(key), zcstring()));
                /* configure key */
                if (tmp) {
                    key = std::move(tmp.dup());
                    debug("jwt key changed to %s", key.cstr);
                }

                /* configure authenticate header string */
                tmp = opts.get(sym(realm), zcstring());
                if (tmp) {
                    authenticate = utils::catstr("Bear realm=\"",tmp, "\"");
                }

                /* configure domain */
                tmp = opts.get(sym(domain), zcstring());
                if (tmp) {
                    domain = tmp.dup();
                }

                /* configure domain */
                tmp = opts.get(sym(path), zcstring());
                if (tmp) {
                    path = tmp.dup();
                }

                if (opts.has(sym(jwt_token_use))) {
                    use = opts.get(sym(jwt_token_use), use);
                }
            }

            template <typename... __A>
            inline void setup(__A... args) {
                auto opts = iod::D(args...);
                configure(opts);
            }

            jwt_authorization()
                : key(rand_8byte_salt()("")),
                  authenticate(zcstring("Bearer").dup())
            {
                debug("generated secret is %s", key.cstr);
            }

        private:
            inline void delete_cookie(response& resp) {
                cookie_t cookie(use.key.cstr);
                cookie.value("");
                cookie.domain(domain.peek());
                cookie.path(path.peek());
                cookie.expires(time(NULL)-2);
                resp.cookie(cookie);
            }
            inline void authrequest(response& resp, const char *msg = "")
            {
                resp.header("WWW-Authenticate", authenticate);
                throw error::unauthorized(msg);
            }

            uint64_t  expiry{3600};
            zcstring  key;
            zcstring  authenticate;
            JwtUse    use;
            // cookie attributes
            zcstring  domain{""};
            zcstring  path{"/"};
        };

        namespace auth {
            template <typename __C>
            static bool getuser(__C& conn, user_t& user) {
                buffer_t qb(32);
                qb << "SELECT * FROM suildb.users WHERE username=";
                __C::params(qb, 1);

                return (conn(qb)(user.username) >> user);
            }

            template <typename __C>
            static bool hasuser(__C& conn, user_t& user) {
                size_t users = 0;
                buffer_t qb(32);
                qb << "SELECT COUNT(username) FROM suildb.users WHERE username=";
                __C::params(qb, 1);

                conn(qb)(user.username) >> users;
                return users != 0;
            }

            template <typename __C>
            static bool hasusers(__C& conn) {
                size_t users = 0;
                conn("SELECT COUNT(username) FROM suildb.users")() >> users;
                return users != 0;
            }

            template <typename __C>
            static bool updateuser(__C& conn, user_t& user) {
                sql::orm<__C,user_t> orm("suildb.users", conn);
                return orm.update(user);
            }

            template <typename __C>
            static void removeuser(__C& conn, user_t& user) {
                sql::orm<__C,user_t> orm("suildb.users", conn);
                orm.remove(user);
            }

            template <typename __C>
            static bool adduser(__C& conn, const zcstring& key, user_t& user) {
                /* generate random salt for user */
                user.salt   = rand_8byte_salt()(nullptr);
                user.passwd = pbkdf2_sha1_hash(key)(user.passwd, user.salt);
                sql::orm<__C,user_t> orm("suildb.users", conn);
                return orm.insert(user);
            }

            template <typename __C>
            static bool updateuser(__C& conn, const zcstring& key, user_t& user) {
                /* generate random salt for user */
                user.salt   = rand_8byte_salt()(nullptr);
                user.passwd = pbkdf2_sha1_hash(key)(user.passwd, user.salt);
                sql::orm<__C,user_t> orm("suildb.users", conn);
                return orm.update(user);
            }

            template <typename __C>
            static bool seedusers(__C& conn, const zcstring& key, std::vector<user_t> users) {
                sql::orm<__C,user_t> orm("suildb.users", conn);
                if (hasusers(conn)) {
                    /* table does not exist */
                    swarn("seeding to non-empty table prohibited");
                    return false;
                }

                for (auto& user : users) {
                    user.salt   = rand_8byte_salt()(nullptr);
                    user.passwd = pbkdf2_sha1_hash(key)(user.passwd, user.salt);
                    if ((orm.insert(user)) != 1) {
                        swarn("seed database table with user '%' failed", user.username.cstr);
                        return false;
                    }
                }

                return true;
            }

            template <typename __C>
            static bool seedusers(__C& conn, const zcstring& key, const char *json) {
                decltype(iod::D(prop(data, std::vector<user_t>))) users;
                zcstring jstr = utils::fs::readall(json);
                if (!jstr) {
                    swarn("reading file %s failed", json);
                    return false;
                }
                iod::json_decode(users, jstr);
                return  seedusers(conn, key, users.data);
            }

            template <typename __C>
            static bool verifyuser(__C& conn, const zcstring& key, user_t& user) {
                user_t u;
                u.username = std::move(user.username);
                if (getuser(conn, u)) {
                    /* user exists */
                    zcstring passwd = pbkdf2_sha1_hash(key)(user.passwd, u.salt);
                    if (passwd == u.passwd) {
                        /* user authorized */
                        user = std::move(u);
                        return user.verified && !user.locked;
                    }
                }

                return false;
            }

            template <typename __C>
            zcstring reguser(__C& conn, const zcstring& key, user_t& user, bool verified = true) {
                if (hasuser(conn, user)) {
                    return utils::catstr("User '", user.username, "' already exists");
                }

                user.verified = verified;
                user.locked = false;
                // add user to database
                if (!adduser(conn, key, user)) {
                    // adding user failed.
                    return "could not register user, contact system admin";
                }

                // user successfully added
                return nullptr;
            }

            template <typename __C, typename __P>
            bool _setuserprops(__C& conn, user_t& user, __P& props) {
                if (props.has(sym(verified)))
                    user.verified = props.get(sym(verified));
                if (props.has(sym(locked)))
                    user.locked = props.get(sym(locked));

                sql::orm<__C,user_t> orm("users", conn);
                return orm.update(user);
            }

            template <typename __C, typename... Props>
            inline bool setuserprops(__C& conn, user_t& user, Props... props) {
                auto opts = iod::D(props...);
                return _setuserprops(conn, user, opts);
            }
        }
    }
}
#endif //SUIL_AUTH_HPP
