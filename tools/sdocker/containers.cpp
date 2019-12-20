//
// Created by dc on 28/09/18.
//

#include "docker.hpp"

namespace suil::docker {

    json::Object Container::ps(bool all, int limit, bool size, std::string& filter)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/json");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (all)       req.args("all", "true");
            if (limit)     req.args("limit", limit);
            if (size)      req.args("size", "true");
            if (!filter.empty()) req.args("filters", filter);

            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }

    json::Object Container::create(const CreateReq &config)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/create");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            req << config;
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::CREATED) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }

    json::Object Container::inspect(const String id, bool size)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/json");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (size) req.args("size", "true");
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }

    json::Object Container::top(const String id, const String args)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/top");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (args) req.args("ps_args", args);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }

    String Container::logs(const String id, const LogsReq& args)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/logs");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (args.follow) req.args("follow",  "true");
            if (args.stdOut) req.args("stdout",   "true");
            if (args.stdErr) req.args("stderr",   "true");
            if (args.since)  req.args("since",    args.since);
            if (args.until)  req.args("until",    args.until);
            if (args.timestamps) req.args("timestamps", args.timestamps);
            if (args.tail)   req.args("tail", args.tail);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() == http::Status::SWITCHING_PROTOCOLS) {
            // switch to streaming
            ierror("Streaming API currently unsupported");
            return nullptr;
        } else if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        return resp.getbody();
    }

    json::Object Container::changes(const suil::String id)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/changes");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource());

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }

    void Container::Export(const suil::String id)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/export");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource());

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Container::stats(const suil::String id, bool stream)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/stats");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (stream) req.args("stream", true);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object obj;
        json::decode(resp(), obj);
        return obj;
    }

    void Container::resize(const suil::String id, uint32_t x, uint32_t y)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/resize");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (x) req.args("x", x);
            if (y) req.args("y", y);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Container::start(const suil::String id, const suil::String detachKeys)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/start");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (detachKeys) req.args("detachKeys", detachKeys);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Container::stop(const suil::String id, uint64_t t)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/stop");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (t) req.args("t", t);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Container::restart(const suil::String id, uint64_t t)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/restart");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (t) req.args("t", t);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Container::kill(const suil::String id, const suil::String sig)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/kill");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (sig) req.args("signal", sig);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Container::update(const suil::String id, const UpdateReq &request)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/update");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            req << request;
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return std::move(respObj);
    }

    void Container::pause(const suil::String id)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/pause");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource());

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Container::unpause(const suil::String id)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/pause");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource());

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Container::wait(const suil::String id, const suil::String condition)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/wait");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (condition) req.args("condition", condition);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return std::move(respObj);
    }

    void Container::remove(const suil::String id, const RemoveQuery &query)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/wait");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::del(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (query.v) req.args("v", "true");
            if (query.force) req.args("force", "true");
            if (query.link)  req.args("link",  "true");
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    String Container::archiveInfo(const suil::String id, const suil::String path)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/archive");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::head(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (path) req.args("path", path);
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }

        auto tmp = resp.hdr("X-Docker-Container-Path-Stat");
        if (tmp.empty())
            return nullptr;
        else
            return String{tmp}.dup();
    }

    json::Object Container::prune(const PruneQuery &filters)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/prune");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            req.args("filters", json::encode(filters));
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return std::move(respObj);
    }

    json::Object Container::exec(const suil::String id, const ExecCreateReq &request)
    {
        auto resource = utils::catstr(ref.apiBase, "/containers/", id, "/exec");
        itrace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            req << request;
            return true;
        });

        itrace("request resource status %d", resp.status());
        if (resp.status() != http::Status::CREATED) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }

}
