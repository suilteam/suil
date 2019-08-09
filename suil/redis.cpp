//
// Created by dc on 11/12/17.
//
#include "redis.h"

namespace suil::redis {

    Transaction::Transaction(BaseClient &client)
            : client(client) {}

    Response &Transaction::exec() {
        // send MULTI command
        if (!in_multi) {
            iwarn("EXEC not support outside of MULTI transaction");
            return cachedresp;
        }

        // send EXEC command
        cachedresp = client.send("EXEC");
        if (!cachedresp) {
            // there is an error on the reply
            ierror("error sending 'EXEC' command: %s", cachedresp.error());
        }

        in_multi = false;
        return cachedresp;
    }

    bool Transaction::discard() {
        if (!in_multi) {
            iwarn("DISCARD not supported outside MULTI");
            return false;
        }

        // send EXEC command
        cachedresp = client.send("DISCARD");
        if (!cachedresp) {
            // there is an error on the reply
            ierror("error sending 'DISCARD' command: %s", cachedresp.error());
        }

        in_multi = false;
        return cachedresp;
    }

    bool Transaction::multi() {
        if (in_multi) {
            iwarn("MULTI not supported within a MULTI block");
            return false;
        }

        // send EXEC command
        cachedresp = client.send("MULTI");
        if (!cachedresp) {
            // there is an error on the reply
            ierror("error sending 'MULTI' command: %s", cachedresp.error());
        }

        in_multi = true;
        return cachedresp;
    }

    Transaction::~Transaction() {
        if (in_multi)
            discard();
    }

    Response BaseClient::dosend(Commmand &cmd, size_t nrps) {
        // send the command to the server
        String data = cmd.prepared();
        size_t size = adaptor().send(data.data(), data.size(), config().timeout);
        if (size != data.size()) {
            // sending failed somehow
            return Response{Reply('-', utils::catstr("sending failed: ", errno_s))};
        }
        adaptor().flush(config().timeout);

        Response resp;
        std::vector<Reply> stage;
        do {
            if (!recvresp(resp.buffer, stage)) {
                // receiving data failed
                return Response{Reply('-',
                                      utils::catstr("receiving Response failed: ", errno_s))};
            }
        } while (--nrps > 0);

        resp.entries = std::move(stage);
        return std::move(resp);
    }

    String BaseClient::commit(Response &resp) {
        // send all the commands at once and read all the responses in one go
        Commmand *last = batched.back();
        batched.pop_back();

        for (auto &cmd: batched) {
            String data = cmd->prepared();
            size_t size = adaptor().send(data.data(), data.size(), config().timeout);
            if (size != data.size()) {
                // sending command failure
                return utils::catstr("sending '", (*cmd)(), "' failed: ", errno_s);
            }
        }

        resp = dosend(*last, batched.size() + 1);
        if (!resp) {
            // return error message
            return resp.error();
        }
        return String{nullptr};
    }

    bool BaseClient::recvresp(OBuffer &out, std::vector<Reply> &stagging) {
        size_t rxd{1}, offset{out.size()};
        char prefix;
        if (!adaptor().read(&prefix, rxd, config().timeout)) {
            return false;
        }

        switch (prefix) {
            case SUIL_REDIS_PREFIX_ERROR:
            case SUIL_REDIS_PREFIX_INTEGER:
            case SUIL_REDIS_PREFIX_VALUE: {
                if (!readline(out)) {
                    return false;
                }
                String tmp{(char *) &out[offset], out.size() - offset, false};
                out << '\0';
                stagging.emplace_back(Reply(prefix, std::move(tmp)));
                return true;
            }
            case SUIL_REDIS_PREFIX_STRING: {
                int64_t len{0};
                if (!readlen(len)) {
                    return false;
                }

                String tmp = String{nullptr};
                if (len >= 0) {
                    size_t size = (size_t) len + 2;
                    out.reserve((size_t) size + 2);
                    if (!adaptor().read(&out[offset], size, config().timeout)) {
                        idebug("receiving string '%lu' failed: %s", errno_s);
                        return false;
                    }
                    // only interested in actual string
                    if (len) {
                        out.seek(len);
                        tmp = String{(char *) &out[offset], (size_t) len, false};
                        // terminate string
                        out << '\0';
                    }
                }
                stagging.emplace_back(Reply(SUIL_REDIS_PREFIX_STRING, std::move(tmp)));
                return true;
            }

            case SUIL_REDIS_PREFIX_ARRAY: {
                int64_t len{0};
                if (!readlen(len)) {
                    return false;
                }

                if (len == 0) {
                    // remove crlf
                    if (!readline(out)) {
                        return false;
                    }
                } else if (len > 0) {
                    int i = 0;
                    for (i; i < len; i++) {
                        if (!recvresp(out, stagging)) {
                            return false;
                        }
                    }
                }

                return true;
            }

            default: {
                ierror("received Response with unsupported type: %c", out[0]);
                return false;
            }
        }
    }

    bool BaseClient::readlen(int64_t &len) {
        OBuffer out{16};
        if (!readline(out)) {
            return false;
        }
        String tmp{(char *) &out[0], out.size(), false};
        tmp.data()[tmp.size()] = '\0';
        utils::cast(tmp, len);
        return true;
    }

    bool BaseClient::readline(OBuffer &out) {
        out.reserve(255);
        size_t cap{out.capacity()};
        do {
            if (!adaptor().receiveuntil(&out[out.size()], cap, "\r", 1, config().timeout)) {
                if (errno == ENOBUFS) {
                    // reserve buffers
                    out.seek(cap);
                    out.reserve(512);
                    continue;
                }
                return false;
            }

            out.seek(cap - 1);
            uint8_t tmp[2];
            cap = sizeof(tmp);
            if (!adaptor().receiveuntil(tmp, cap, "\n", 1, config().timeout)) {
                // this should be impossible
                return false;
            }
            break;
        } while (true);

        return true;
    }

    bool BaseClient::info(ServerInfo &out) {
        Commmand cmd("INFO");
        Response resp = send(cmd);
        if (!resp) {
            // couldn't receive Response
            ierror("failed to receive server information");
            return false;
        }

        Reply &rp = resp.entries[0];
        auto parts = rp.data.split("\r");
        for (auto &part: parts) {
            if (*part == '\n') part++;
            if (part[0] == '#' || strlen(part) == 0) continue;

            char *k = part;
            char *v = strchr(part, ':');
            v[0] = '\0';
            String key{k, (size_t) (v - k) - 1, false};
            v++;
            String val{v, strlen(v), false};

            if (key.compare("redis_version") == 0) {
                out.version = std::move(val);
            } else {
                out.params.emplace(std::make_pair(
                        std::move(key), std::move(val)));
            }
        }

        return true;
    }

    int64_t BaseClient::incr(const String& key, int by) {
        Response resp;
        if (by == 0)
            resp = send("INCR", key);
        else
            resp = send("INCRBY", key, by);

        if (!resp) {
            throw Exception::create("redis INCR '", key,
                                    "' failed: ", resp.error());
        }
        return (int64_t) resp;
    }

    int64_t BaseClient::decr(const String& key, int by) {
        Response resp;
        if (by == 0)
            resp = send("DECR", key);
        else
            resp = send("DECRBY", key, by);

        if (!resp) {
            throw Exception::create("redis DECR '", key,
                                    "' failed: ", resp.error());
        }
        return (int64_t) resp;
    }

    String BaseClient::substr(const String& key, int start, int end) {
        Response resp = send("SUBSTR", key, start, end);
        if (resp) {
            return resp.get<String>(0);
        }
        throw Exception::create("redis SUBSTR '", key,
                                "' start=", start, ", end=",
                                end, " failed: ", resp.error());
    }

    bool BaseClient::exists(const suil::String &key) {
        Response resp = send("EXISTS", key);
        if (resp) {
            return (int) resp != 0;
        }
        else {
            throw Exception::create("redis EXISTS '", key,
                                    "' failed: ", resp.error());
        }
    }

    bool BaseClient::del(const String& key) {
        Response resp = send("DEL", key);
        if (resp) {
            return (int) resp != 0;
        }
        else {
            throw Exception::create("redis DEL '", key,
                                    "' failed: ", resp.error());
        }
    }

    std::vector<String> BaseClient::keys(const String& pattern) {
        Response resp = send("KEYS", pattern);
        if (resp) {
            std::vector<String> tmp = resp;
            return std::move(tmp);
        }
        else {
            throw Exception::create("redis KEYS pattern = '", pattern,
                                    "' failed: ", resp.error());
        }
    }

    int BaseClient::expire(const String& key, int64_t secs) {
        Response resp =  send("EXPIRE", key, secs);
        if (resp) {
            return (int) resp;
        }
        else {
            throw Exception::create("redis EXPIRE  '", key, "' secs ", secs,
                                    " failed: ", resp.error());
        }
    }

    int BaseClient::ttl(const String& key) {
        Response resp =  send("TTL", key);
        if (resp) {
            return (int) resp;
        }
        else {
            throw Exception::create("redis TTL  '", key,
                                    "' failed: ", resp.error());
        }
    }

    int BaseClient::llen(const String& key) {
        Response resp = send("LLEN", key);
        if (resp) {
            return (int) resp;
        }
        else {
            throw Exception::create("redis LLEN  '", key,
                                    "' failed: ", resp.error());
        }
    }

    bool BaseClient::ltrim(const String& key, int start, int end) {
        Response resp = send("LTRIM", key, start, end);
        if (resp) {
            return resp.status();
        }
        else {
            throw Exception::create("redis LTRIM  '", key,
                                    "' failed: ", resp.error());
        }
    }

    int BaseClient::hdel(const String& hash, const String& key) {
        Response resp = send("HDEL", hash, key);
        if (resp) {
            return (int) resp;
        }
        else {
            throw Exception::create("redis HDEL  '", hash, ":", key,
                                    "' failed: ", resp.error());
        }
    }

    bool BaseClient::hexists(const String& hash, const String& key) {
        Response resp = send("HEXISTS", hash, key);
        if (resp) {
            return (int) resp != 0;
        }
        else {
            throw Exception::create("redis HEXISTS  '", hash, ":", key,
                                    "' failed: ", resp.error());
        }
    }

    std::vector<String> BaseClient::hkeys(const String& hash) {
        Response resp = send("HKEYS", hash);
        if (resp) {
            std::vector<String> keys = resp;
            return  std::move(keys);
        }
        else {
            throw Exception::create("redis HKEYS  '", hash,
                                    "' failed: ", resp.error());
        }
    }

    ServerInfo::ServerInfo(ServerInfo &&o)
        : version(std::move(o.version)),
          buffer(std::move(o.buffer)),
          params(std::move(o.params))
    {}

    ServerInfo& ServerInfo::operator=(ServerInfo &&o) {
        version = std::move(o.version);
        buffer  = std::move(o.buffer);
        params  = std::move(o.params);
        return *this;
    }

    const String& ServerInfo::operator[](const suil::String &key) const {
        auto data = params.find(key);
        if (data != params.end()) {
            return data->second;
        }
        throw Exception::create("parameter '", key, "' does not exist");
    }
}