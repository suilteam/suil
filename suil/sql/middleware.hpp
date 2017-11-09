//
// Created by dc on 8/2/17.
//

#ifndef SUIL_MIDDLEWARE_HPP
#define SUIL_MIDDLEWARE_HPP

#include <suil/sys.hpp>
#include <suil/sql/orm.hpp>
#include <suil/http/response.hpp>
#include <suil/http/request.hpp>

namespace suil {
    namespace sql {

        template <typename __Db>
        struct middleware {
            template <typename __O>
            using orm = sql::orm<typename __Db::Connection, __O>;

            struct Context{
            };

            template <typename __Opts>
            void configure(__Opts& opts, const char *conn) {
                db.configure(opts, conn);
                expires = opts.get(sym(expires), 5000);
            }

            template <typename... __Opts>
            void setup(const char *connstr, __Opts... opts) {
                auto options = iod::D(opts...);
                configure(options, connstr);
            }

            typename __Db::Connection& conn() {
                return db.connection();
            }

            void before(http::request&, http::response&, Context&) {
            }

            void after(http::request&, http::response&, Context&) {
            }

        private:
            __Db    db;
            int     expires{-1};
        };
    }
}
#endif //SUIL_MIDDLEWARE_HPP