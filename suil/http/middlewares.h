//
// Created by dc on 29/11/18.
//

#ifndef SUIL_MIDDLEWARES_H
#define SUIL_MIDDLEWARES_H

#include <suil/redis.h>
#include <suil/channel.h>
#include <suil/http/auth.h>
#include <suil/http/routing.h>

namespace suil::http::mw {

    define_log_tag(REDIS_MW);

    template <typename Proto>
    struct _Redis : LOGGER(REDIS_MW) {
        using Protocol = Proto;
        using Db = redis::RedisDb<Proto>;
        using Connection = redis::Client<Proto>;

        struct Context {

            inline Connection& conn(int db = 0) {
                if (self == nullptr) {
                    /* shouldn't happen if used correctly */
                    throw Exception::create("Redis middleware context should be accessed by routes or any middlewares after Redis");
                }
                return self->conn(db);
            }

        private:
            template <typename P>
            friend struct _Redis;
            _Redis<Proto>             *self{nullptr};
        };

        void before(http::Request&, http::Response&, Context& ctx) {
            /* we just initialize context here, we don't create any connections */
            ctx.self = this;
        }

        void after(http::Request& req, http::Response&, Context& ctx) {
            /* clear all cached connections for given request */
            ctx.self = nullptr;
        }

        template <typename Opts>
        void configure(const char *host, int port, Opts& opts) {
            database.configure(host, port, opts);
        }

        template <typename... Opts>
        void setup(const char *host, int port, Opts... opts) {
            auto options = iod::D(std::forward<Opts>(opts)...);
            configure(host, port, options);
        }

        inline Connection& conn(int db = 0) {
            itrace("connecting to database %d", db);
            return database.connect(db);
        }

    private:
        Db database{};
    };

    using RedisSsl = _Redis<SslSock>;
    using Redis    = _Redis<TcpSock>;

    template <typename Proto=TcpSock>
    struct _JwtSession {

        using RedisContext =  typename http::mw::_Redis<Proto>::Context;

        struct Context {
            bool authorize(http::Jwt&& jwt) {
                /* pass token to JWT authorization middleware */
                jwtContext->authorize(std::move(jwt));
                /* store generated token in database */
                scoped(conn, redisContext->conn(self->sessionDb));
                if (!conn.set(jwtContext->jwtRef().aud().peek(), jwtContext->token().peek())) {
                    /* saving token fail */
                    jwtContext->authenticate("Creating session failed");
                    return false;
                }

                auto expires = jwtContext->jwtRef().exp();
                if (expires > 0) {
                    // set token to expire
                    conn.expire(jwtContext->jwtRef().aud().peek(), expires-time(NULL));
                }

                return true;
            }

            bool authorize(const String& user) {
                /* fetch token and try authorizing the token */
                scoped(conn, redisContext->conn(self->sessionDb));
                return authorize(conn, user);
            }

            inline void revoke(const http::Jwt& jwt) {
                Ego.revoke(jwt.aud());
            }

            inline void revoke(const String& user) {
                /* revoke token for given user */
                scoped(conn, redisContext->conn(self->sessionDb));
                revoke(conn, user);
            }

        private:
            void revoke(typename _Redis<Proto>::Connection& conn, const String& user) {
                conn.del(user);
                auto pattern = utils::catstr("*", user);
                auto keys = conn.keys(pattern);
                for (auto& key: keys) {
                    // delete all the other keys
                    conn.del(key);
                }
                jwtContext->logout();
            }

            bool authorize(typename _Redis<Proto>::Connection& conn, const String& user) {
                if (!conn.exists(user.peek())) {
                    /* session does not exist */
                    return false;
                }

                auto token = conn.template get<String>(user.peek());
                if (!jwtContext->authorize(token)) {
                    /* token invalid, revoke the token */
                    Ego.revoke(conn, user);
                    return false;
                }

                return true;
            }

            friend struct _JwtSession;
            http::JwtAuthorization::Context*  jwtContext{nullptr};
            RedisContext* redisContext{nullptr};
            _JwtSession<Proto>*               self{nullptr};
        };

        template <typename Contexts>
        void before(http::Request& req, http::Response& resp, Context& ctx, Contexts& ctxs) {
            /* Only applicable on authorized routes with tokens */
            ctx.self = this;
            http::JwtAuthorization::Context& jwtContext = ctxs.template get<http::JwtAuthorization>();
            RedisContext& redisContext = ctxs.template get<http::mw::_Redis<Proto>>();
            ctx.jwtContext   = &jwtContext;
            ctx.redisContext = &redisContext;

            if (!jwtContext.token().empty()) {
                /* request has authorization token, ensure that is still valid */
                auto aud = jwtContext.jwtRef().aud().peek();
                scoped(conn, redisContext.conn(Ego.sessionDb));
                if (!conn.exists(aud.peek())) {
                    /* token does not exist */
                    jwtContext.authenticate("Attempt to access protected resource with invalid token");
                    resp.end();
                    return;
                }

                auto cachedToken = conn.template get<String>(aud.peek());
                if (cachedToken != jwtContext.token()) {
                    /* Token bad or token has been revoked */
                    jwtContext.authenticate("Attempt to access protected resource with invalid token");
                    resp.end();
                    return;
                }
                ctx.jwtContext   = &jwtContext;
                ctx.redisContext = &redisContext;
            }
        }

        void after(http::Request& req, http::Response&, Context& ctx) {
            /* also only applicable when the */
        }

    private:
        int sessionDb{1};
    };

    using JwtSession    = _JwtSession<TcpSock>;
    using JwtSessionSsl = _JwtSession<SslSock>;

    struct Initializer final {
        using Handler = std::function<bool(const http::Request&, http::Response&)>;
        struct Context {
        };

        void before(http::Request& req, http::Response& resp, Context& ctx);

        void after(http::Request& req, http::Response& resp, Context& ctx);

        template <typename Ep>
        void setup(Ep& ep, Handler handler, bool form = true) {
            if (handler == nullptr) {
                blocked = false;
                return;
            }

            if (Ego.handler == nullptr) {
                initRoute =
                ep.dynamic("/app-init")
                ("GET"_method, "POST"_method, "OPTIONS"_method)
                .attrs(opt(AUTHORIZE, Auth{false}), opt(PARSE_FORM, form))
                ([&](const http::Request& req, http::Response& resp) { init(req, resp); });
                blocked =  true;
                Ego.handler = handler;
            }
        }

    private:
        void init(const http::Request& req, http::Response& resp);
        bool  blocked{false};
        bool  unblock{false};
        int32_t initRoute{0};
        Handler  handler{nullptr};
    };

    define_log_tag(ENDPOINT_ADMIN);
    struct EndpointAdmin final : LOGGER(ENDPOINT_ADMIN) {
        static constexpr const char* Role = "SystemAdmin";

        struct Context {
        };

        void before(http::Request& req, http::Response& resp, Context& ctx) {}

        void after(http::Request& req, http::Response& resp, Context& ctx) {}

        template <typename Endpoint>
        void setup(Endpoint& ep) {
            idebug("initializing administration endpoint");

            ep("/_admin/routes")
            ("GET"_method, "OPTIONS"_method)
            .attrs(opt(AUTHORIZE, Auth{Role}))
            ([&ep]() {
                std::vector<route_schema_t> schemas;
                ep.router.enumerate([&schemas](BaseRule& rule) {
                    if (rule.path().find("_admin") != std::string::npos) {
                        // do not include admin routes
                        return true;
                    }

                    schemas.emplace_back(rule.schema());
                    return true;
                });

                return schemas;
            });

            ep("/_admin/route/{uint}")
            ("GET"_method, "OPTIONS"_method)
            .attrs(opt(AUTHORIZE, Auth{Role}))
            ([&ep](const Request&, Response& resp, uint32_t id) {
                auto status{Status::NO_CONTENT};
                ep.router.enumerate([&ep, &resp, &status, id](BaseRule& rule) {
                    if (rule.path().find("_admin") != std::string::npos) {
                        // do not include admin routes
                        return true;
                    }

                    if (rule.id() == id) {
                        route_schema_t schema{};
                        rule.schema(schema);
                        resp << schema;
                        status = Status::OK;
                        return false;
                    }
                    return true;
                });

                resp.end(status);
            });

            ep("/_admin/route/{uint}/enable")
            ("POST"_method, "OPTIONS"_method)
            .attrs(opt(AUTHORIZE, Auth{Role}))
            ([this, &ep](uint32_t id) {
                return setRouteEnabled(ep, id, true);
            });

            ep("/_admin/route/{uint}/disable")
            ("POST"_method, "OPTIONS"_method)
            .attrs(opt(AUTHORIZE, Auth{Role}))
            ([this, &ep](uint32_t id) {
                return setRouteEnabled(ep, id, false);
            });

            ep("/_admin/endpoint/stats")
            ("GET"_method, "OPTIONS"_method)
            .attrs(opt(AUTHORIZE, Auth{Role}))
            ([&ep]{
                return ep.stats;
            });

            ep("/_admin/about")
            ("GET"_method, "OPTIONS"_method)
            .attrs(opt(AUTHORIZE, Auth{Role}))
            ([&ep]{
                return utils::catstr(version::SWNAME, "-", version::STRING);
            });

            ep("/_admin/version")
            ("GET"_method, "OPTIONS"_method)
            .attrs(opt(AUTHORIZE, Auth{Role}))
            ([&ep]{
                return ver_json;
            });
        }

    private:
        template <typename  Endpoint>
        Status setRouteEnabled(Endpoint& ep, uint32_t id, bool en) {
            auto status{Status::NO_CONTENT};
            ep.router.enumerate([this, &status, id, en](BaseRule& rule) {
                if (rule.path().find("_admin") != std::string::npos) {
                    // do not include admin routes
                    return true;
                }

                if (rule.id() == id) {
                    rule.attrs_.ENABLED = en;
                    status = Status::ACCEPTED;
                    idebug("disabling route %s", rule.path().c_str());
                    return false;
                }
                return true;
            });
            return status;
        }
    };
}

#endif //SUIL_MIDDLEWARES_H
