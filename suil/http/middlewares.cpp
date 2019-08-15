//
// Created by dc on 29/11/18.
//

#include "middlewares.h"

namespace suil::http::mw {

    void Initializer::before(Request &req, Response &resp, Context& ctx) {
        if (!blocked) {
            if (!req.enabled()) {
                resp.end(http::Status::NOT_FOUND);
            }
            return;
        }

        if (req.routeid() != initRoute) {
            // reject request
            resp.end(http::Status::SERVICE_UNAVAILABLE);
        }
    }

    void Initializer::after(Request &req, Response &resp, Context& ctx)
    {
        if (blocked) {
            if (unblock) {
                blocked = false;
                req.enabled(false);
                unblock =  false;
            }
        }
    }

    void Initializer::init(const Request &req, Response &resp)
    {
        if (handler != nullptr) {
            unblock = handler(req, resp);
        }
    }
}