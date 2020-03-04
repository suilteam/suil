//
// Created by dc on 28/06/17.
//

#ifndef SUIL_ENDPOINT_HPP
#define SUIL_ENDPOINT_HPP

#include <suil/symbols.h>
#include <suil/net.h>
#include <suil/http/connection.h>
#include <suil/http/routing.h>

#include <typeindex>

namespace suil {

    namespace http {

        namespace mw {
            struct EndpointAdmin;
        }

        define_log_tag(HTTP_SERVER);

        template<typename H, typename B = TcpSs, typename... Mws>
        struct BaseServer : LOGGER(HTTP_SERVER) {
            typedef BaseServer<H, B, Mws...> server_t;

            struct socket_handler {
                void operator()(SocketAdaptor &sock, server_t *s) {
                    Connection<H, Mws...> conn(
                            sock, s->config, s->handler, &s->mws, s->stats);

                    conn.start();
                }
            };

            inline void stop() {
                /* stop the backend */
                backend.stop();
            }

            inline int listen() {
                return backend.listen();
            }

            typedef Server<socket_handler, B, server_t> raw_server_t;
            typedef std::tuple<Mws...> middlewares_t;

        protected:
            friend struct mw::EndpointAdmin;
            template<typename... Opts>
            BaseServer(H &h, Opts&... opts)
                : backend(config, config, this),
                  handler(h)
            {
                utils::apply_config(config, opts...);
                initialize();
            }

            BaseServer(H &h)
                    : backend(config, config, this),
                      handler(h)
            {
                initialize();
            }

            inline int run() {
                /* start the backend server */
                return backend.run();
            }

            void initialize() {
                backend.init();

                stats.rx_bytes = 0;
                stats.tx_bytes = 0;
                stats.total_requests = 0;
                stats.open_requests  = 0;
            }

            raw_server_t        backend;
            HttpConfig       config;
            H&                handler;
            middlewares_t       mws;
            ServerStats      stats;
        };

        #define eproute(app, url) \
            app.route<suil::magic::get_parameter_tag(suil::magic::const_str(url))>(url)

        template <typename B = TcpSs, typename... Mws>
        struct Endpoint : public BaseServer<Router, B, Mws...> {
            typedef BaseServer<Router, B, Mws...> basesrv_t;
            using unique_ptr = std::unique_ptr<Endpoint<B, Mws...>>;
            /**
             * creates a new http endpoint which handles http requests
             * @param conf the configuration
             */
            Endpoint(std::string api)
                    : router(api),
                      basesrv_t(router)
            {}

            template <typename... __Opts>
            Endpoint(std::string api, __Opts... opts)
                : router(api),
                  basesrv_t(router, opts...)
            {}

            int start() {
                // configure pid
                this->stats.pid = spid;

                router.validate();

                itrace("starting server...");
                return basesrv_t::run();
            }

            template <typename __Opts>
            inline void configure(__Opts& opts) {
                /* apply endpoint configuration */
                utils::apply_options(this->config, opts);
            }

            DynamicRule& dynamic(std::string&& rule) {
                return router.new_rule_dynamic(std::move(rule));
            }

            template<uint64_t Tag>
            auto route(std::string&& rule)
            -> typename std::result_of<decltype(&Router::new_rule_tagged<Tag>)(Router, std::string&&)>::type
            {
                return router.new_rule_tagged<Tag>(std::move(rule));
            }

            DynamicRule&  operator()(std::string&& rule)
            {
                return dynamic(std::move(rule));
            }

            using context_t = detail::context<Mws...>;
            template <typename T>
            typename T::Context& context(const Request& req)
            {
                static_assert(magic::contains<T, Mws...>::value, "not Middleware in endpoint");
                auto& ctx = *reinterpret_cast<context_t*>(req.middleware_context);
                return ctx.template get<T>();
            }

            template <typename T>
            T& middleware()
            {
                return get_element_by_type<T, Mws...>(basesrv_t::mws);
            }

            inline const std::string& getApiBaseRoute() const {
                return router.apiBaseRoute();
            }

            const HttpConfig& getConfig() const {
                return Ego.config;
            }

            inline void dump() {
                router.debug_print();
            }

            struct Controller {
                sptr(Controller);

                Controller(Endpoint& ep)
                    : api(ep)
                {}

                virtual void init() {};

                template <typename... Args>
                static void fail(http::Response& resp, const String& key, Args... args) {
                    resp << "{\"status\":\"" << key << "\", \"msg\":\"";
                    if constexpr (sizeof...(args)) {
                        Controller::append(resp, std::forward<Args>(args)...);
                    }
                    resp << "\"}";
                }

            protected:
                Endpoint& api;

            private:
                template <typename Arg, typename... Args>
                static void append(http::Response& resp, Arg arg, Args... args) {
                    resp << arg;
                    if constexpr (sizeof...(args)) {
                        Controller::append(resp, std::forward<Args>(args)...);
                    }
                }
            };

        private:
            friend struct mw::EndpointAdmin;
            Router   router;
        };

        namespace opts {
            template<typename... T>
            auto http_endpoint(T &&... s) {
                return sym(http_endpoint) = std::make_tuple(s...);
            }
        }

        template <typename... Mws>
        using TcpEndpoint = Endpoint<TcpSs, Mws...>;
        template <typename... Mws>
        using SslEndpoint = Endpoint<SslSs, Mws...>;
    }
}

#endif //SUIL_ENDPOINT_HPP
