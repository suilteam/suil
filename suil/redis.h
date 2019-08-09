//
// Created by dc on 11/12/17.
//
// Modified implementation of https://github.com/octo/credis

#ifndef SUIL_REDIS_HPP
#define SUIL_REDIS_HPP

#include <deque>
#include <list>
#include <suil/channel.h>
#include <suil/net.h>
#include <suil/blob.h>

namespace suil {

#define SUIL_REDIS_CRLF                 "\r\n"
#define SUIL_REDIS_STATUS_OK            "OK"
#define SUIL_REDIS_PREFIX_ERROR         '-'
#define SUIL_REDIS_PREFIX_VALUE         '+'
#define SUIL_REDIS_PREFIX_STRING        '$'
#define SUIL_REDIS_PREFIX_ARRAY         '*'
#define SUIL_REDIS_PREFIX_INTEGER       ':'

    namespace redis {
        define_log_tag(REDIS);

        struct Commmand {
            template <typename... Params>
            Commmand(const char *cmd, Params... params)
                : buffer(32+strlen(cmd))
            {
                prepare(cmd, params...);
            }

            String operator()() const {
                return std::move(String(buffer.data(),
                                          MIN(64, buffer.size()),
                                          false).dup());
            }

            template <typename Param>
            Commmand& operator<<(const Param param) {
                addparam(param);
                return *this;
            }

            template <typename... Params>
            void addparams(Params&... params) {
                addparam(params...);
            }

        private:
            friend struct BaseClient;

            String prepared() const {
                return String{buffer.data(), buffer.size(), false};
            }

            template <typename... Params>
            void prepare(const char *cmd, Params&... params) {
                buffer << SUIL_REDIS_PREFIX_ARRAY << (1+sizeof...(Params)) << SUIL_REDIS_CRLF;
                addparams(cmd, params...);
            }

            void addparam() {}

            template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
            void addparam(const T& param) {
                std::string str = std::to_string(param);
                String tmp(str.data(), str.size(), false);
                addparam(tmp);
            }

            void addparam(const char* param) {
                String str(param);
                addparam(str);
            }

            void addparam(const String& param) {
                buffer << SUIL_REDIS_PREFIX_STRING << param.size() << SUIL_REDIS_CRLF;
                buffer << param << SUIL_REDIS_CRLF;
            }

            void addparam(const Data& param) {
                size_t  sz =  param.size() << 1;
                buffer << SUIL_REDIS_PREFIX_STRING << sz << SUIL_REDIS_CRLF;
                buffer.reserve(sz);
                sz = utils::hexstr(param.cdata(), param.size(),
                                   &buffer.data()[buffer.size()], buffer.capacity());
                buffer.seek(sz);
                buffer << SUIL_REDIS_CRLF;
            }

            template <size_t B>
            void addparam(const Blob<B>& param) {
                auto raw = &param.cbin();
                size_t  sz =  param.size() << 1;
                buffer << SUIL_REDIS_PREFIX_STRING << sz << SUIL_REDIS_CRLF;
                buffer.reserve(sz);
                sz = utils::hexstr(raw, param.size(),
                                   &buffer.data()[buffer.size()], buffer.capacity());
                buffer.seek(sz);
                buffer << SUIL_REDIS_CRLF;
            }

            template <typename T, typename... Params>
            void addparam(const T& param, const Params&... params) {
                addparam(param);
                addparam(params...);
            }

            OBuffer buffer;
        };

        struct Reply {
            Reply(char prefix, String&& data)
                : prefix(prefix),
                  data(data)
            {}

            Reply(OBuffer&& rxb)
                : recvd(std::move(rxb)),
                  prefix(rxb.data()[0]),
                  data(&rxb.data()[1], rxb.size()-3, false)
            {
                // null terminate
                data.data()[data.size()] = '\0';
            }

            operator bool() const {
                return (prefix != SUIL_REDIS_PREFIX_ERROR && !data.empty());
            }

            inline bool status(const char *expect = SUIL_REDIS_STATUS_OK) const {
                return (prefix == SUIL_REDIS_PREFIX_VALUE) && (data.compare(expect) == 0);
            }

            const char* error() const {
                if (prefix == SUIL_REDIS_PREFIX_ERROR) return  data.data();
                return "";
            }

            bool special() const {
                return prefix == '[' || prefix == ']';
            }

        private:
            friend struct Response;
            friend struct BaseClient;
            String data{nullptr};
            OBuffer recvd;
            char     prefix{'-'};
        };

        struct Response {

            Response(){}

            Response(Reply&& rp)
            {
                entries.emplace_back(std::move(rp));
            }

            operator bool() const {
                if (entries.empty()) return false;
                else if(entries.size() > 1) return true;
                else  return entries[0];
            }

            const char* error() const {
                if (entries.empty()) {
                    return "";
                } else if(entries.size() > 1){
                    return entries[entries.size()-1].error();
                }
                else {
                    return entries[0].error();
                }
            }

            inline bool status(const char *expect = SUIL_REDIS_STATUS_OK) {
                return !entries.empty() && entries[0].status(expect);
            }

            template <typename T>
            const T get(int index = 0) const {
                if (index > entries.size())
                    throw Exception::create("index '", index,
                                             "' out of range '", entries.size(), "'");
                T d{};
                castreply(index, d);
                return std::move(d);

            }

            template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type * = nullptr>
            operator std::vector<T>() const {
                std::vector<T> tmp;
                for (int i=0; i < 0; i++) {
                    tmp.push_back(get<T>(i));
                }
                return  std::move(tmp);
            }

            template <typename T, typename std::enable_if<!std::is_arithmetic<T>::value>::type * = nullptr>
            operator std::vector<T>() const {
                std::vector<String> tmp;
                for (int i=0; i < 0; i++) {
                    tmp.emplace_back(get<String >(i));
                }
                return  std::move(tmp);
            }

            template <typename T>
            operator T() const {
                return  std::move(get<T>(0));
            }

            const String peek(int index = 0) {
                if (index > entries.size())
                    throw Exception::create("index '", index,
                                             "' out of range '", entries.size(), "'");
                return entries[0].data.peek();
            }

            void operator|(std::function<bool(const String&)> f) {
                for(auto& rp : entries) {
                    if (!f(rp.data)) break;
                }
            }

        private:

            template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
            void castreply(int idx, T& d) const {
                const String& tmp = entries[idx].data;
                utils::cast(tmp, d);
            }

            void castreply(int idx, String & d) const {
                const String& tmp = entries[idx].data;
                d = tmp.dup();
            }

            friend struct BaseClient;
            friend struct Transaction;

            std::vector<Reply> entries;
            OBuffer           buffer{128};
        };

        struct redisdb_config {
            int64_t     timeout{-1};
            std::string passwd{""};
            uint64_t    keep_alive{30000};
        };

        struct ServerInfo {
            String    version;
            ServerInfo(){}

            ServerInfo(const ServerInfo& o) = delete;
            ServerInfo&operator=(const ServerInfo& o) = delete;

            ServerInfo(ServerInfo&& o);

            ServerInfo&operator=(ServerInfo&& o);

            inline const String&operator[](const char *key) const {
                return (*this)[String{key}];
            }

            const String&operator[](const String& key) const;

        private:
            friend  struct BaseClient;
            CaseMap<String> params;
            OBuffer      buffer;
        };

        struct BaseClient : LOGGER(REDIS) {
            inline Response send(Commmand& cmd) {
                return dosend(cmd, 1);
            }

            template <typename... Args>
            Response send(const String& cd, Args... args) {
                Commmand cmd(cd(), args...);
                return std::move(send(cmd));
            }

            template <typename... Args>
            Response operator()(String&& cd, Args... args) {
                return send(cd, args...);
            }

            inline void flush() {
                reset();
            }

            inline bool auth(const char *pass) {
                return auth(String{pass});
            }

            bool auth(const String& pass) {
                Commmand cmd("AUTH", pass);
                auto resp = send(cmd);
                return resp.status();
            }

            bool ping() {
                Commmand cmd("PING");
                return send(cmd).status("PONG");
            }

            template <typename T>
            auto get(const String& key) -> T {
                Response resp = send("GET", key);
                if (!resp) {
                    throw Exception::create("redis GET '", key,
                                             "' failed: ", resp.error());
                }
                return (T) resp;
            }

            template <typename T>
            inline bool set(const String& key, const T val) {
                return send("SET", key, val).status();
            }

            int64_t incr(const String& key, int by = 0);

            int64_t decr(const String& key, int by = 0);

            template <typename T>
            inline bool append(const String& key, const T val) {
                return send("APPEND", key, val);
            }

            String substr(const String& key, int start, int end);

            bool exists(const String& key);

            bool del(const String& key);

            std::vector<String> keys(const String& pattern);

            int expire(const String& key, int64_t secs);

            int ttl(const String& key);

            template <typename... T>
            int rpush(const String& key, const T... vals) {
                Response resp = send("RPUSH", key, vals...);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis RPUSH  '", key,
                                             "' failed: ", resp.error());
                }
            }

            template <typename... T>
            int lpush(const String& key, const T... vals) {
                Response resp = send("LPUSH", key, vals...);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis LPUSH  '", key,
                                             "' failed: ", resp.error());
                }
            }

            int llen(const String& key);

            template <typename T>
            auto lrange(const String& key, int start = 0, int end = -1) -> std::vector<T> {
                Response resp = send("LRANGE", key, start, end);
                if (resp) {
                    std::vector<T> tmp = resp;
                    return  std::move(tmp);
                }
                else {
                    throw Exception::create("redis LRANGE  '", key,
                                             "' failed: ", resp.error());
                }
            }

            bool ltrim(const String& key, int start = 0, int end = -1);

            template <typename T>
            auto ltrim(const String& key, int index) -> T {
                Response resp = send("LINDEX", key, index);
                if (resp) {
                    return (T) resp;
                }
                else {
                    throw Exception::create("redis LINDEX  '", key,
                                             "' failed: ", resp.error());
                }
            }

            int hdel(const String& hash, const String& key);

            bool hexists(const String& hash, const String& key);

            std::vector<String> hkeys(const String& hash);

            template <typename T>
            auto hvals(const String& hash) -> std::vector<T> {
                Response resp = send("HVALS", hash);
                if (resp) {
                    std::vector<T> vals = resp;
                    return  std::move(vals);
                }
                else {
                    throw Exception::create("redis HVALS  '", hash,
                                            "' failed: ", resp.error());
                }
            }

            template <typename T>
            auto hget(const String& hash, const String& key) -> T {
                Response resp = send("HGET", hash, key);
                if (!resp) {
                    throw Exception::create("redis HGET '", hash, ":", key,
                                            "' failed: ", resp.error());
                }
                return (T) resp;
            }

            template <typename T>
            inline bool hset(const String& hash, const String& key, const T val) {
                Response resp = send("HSET", hash, key, val);
                if (!resp) {
                    throw Exception::create("redis HSET '", hash, ":", key,
                                            "' failed: ", resp.error());
                }
                return (int) resp != 0;
            }

            inline size_t hlen(const String& hash) {
                return (size_t) send("HLEN", hash);
            }

            template <typename... T>
            int sadd(const String& set, const T... vals) {
                Response resp = send("SADD", set, vals...);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis SADD  '", set,
                                            "' failed: ", resp.error());
                }
            }

            template <typename T>
            auto smembers(const String& set) -> std::vector<T>{
                Response resp = send("SMEMBERS", set);
                if (resp) {
                    std::vector<T> vals = resp;
                    return  std::move(vals);
                }
                else {
                    throw Exception::create("redis SMEMBERS  '", set,
                                            "' failed: ", resp.error());
                }
            }

            template <typename T>
            auto spop(const String& set) -> T {
                Response resp = send("spop", set);
                if (!resp) {
                    throw Exception::create("redis spop '", set,
                                            "' failed: ", resp.error());
                }
                return (T) resp;
            }


            inline size_t scard(const String& set) {
                return (size_t) send("SCARD", set);
            }


            bool info(ServerInfo&);

            virtual void close() {};

        protected:
            BaseClient() = default;

            BaseClient(BaseClient&& o) = default;

            BaseClient&operator=(BaseClient&& o) = default;

            BaseClient(const BaseClient&) = delete;
            BaseClient&operator=(const BaseClient&) = delete;

            virtual  SocketAdaptor& adaptor() = 0;
            virtual  redisdb_config& config() = 0;

            Response dosend(Commmand& cmd, size_t nreply);

            friend struct Transaction;
            inline void reset() {
                batched.clear();
            }
            inline void batch(Commmand& cmd) {
                batched.push_back(&cmd);
            }
            inline void batch(std::vector<Commmand>& cmds) {
                for (auto& cmd: cmds) {
                    batch(cmd);
                }
            }

            bool recvresp(OBuffer& out, std::vector<Reply>& stagging);

            bool readline(OBuffer& out);

            bool readlen(int64_t& len);

            String commit(Response& resp);

            std::vector<Commmand*> batched;
        };

        template <typename Sock>
        struct Client : BaseClient {
            using CacheId = typename std::list<Client<Sock>>::iterator;
            using CloseHandler = std::function<void(CacheId, bool)>;

            Client(Sock&& insock, redisdb_config& config, CloseHandler onClose)
                : sock(std::move(insock)),
                  closeHandler(onClose),
                  configRef(config)
            {}

            Client() = default;

            Client(Client&& o)
                : BaseClient(std::move(o)),
                  sock(std::move(o.sock)),
                  closeHandler(o.closeHandler),
                  cacheId(o.cacheId),
                  configRef(o.configRef)
            {
                o.closeHandler = nullptr;
                o.cacheId = CacheId{nullptr};
            }

            Client&operator=(Client&& o) {
                BaseClient::operator=(std::move(o));
                sock = std::move(o.sock);
                closeHandler = o.closeHandler;
                cacheId = o.cacheId;
                configRef  = o.configRef;

                o.closeHandler = nullptr;
                o.cacheId = CacheId{nullptr};

                return *this;
            }

            Client&operator=(const Client&) = delete;
            Client(const Client&) = delete;

            virtual void close() override {
                if (closeHandler && (cacheId != CacheId{nullptr})) {
                    closeHandler(cacheId, false);
                }
            }

            ~Client() {
                // reset and close Connection
                if (closeHandler && (cacheId != CacheId{nullptr})) {
                    closeHandler(cacheId, true);
                    closeHandler = nullptr;
                }

                reset();
                sock.close();
            }

            virtual SocketAdaptor& adaptor() override  {
                return sock;
            }

            virtual redisdb_config& config() override {
                return configRef;
            }

        private:
            template <typename T>
            friend struct RedisDb;
            Sock         sock;
            CacheId      cacheId{nullptr};
            CloseHandler closeHandler{nullptr};
            redisdb_config& configRef;
        };

        template <typename Proto = TcpSock>
        struct RedisDb : LOGGER(REDIS) {
        private:
            using ConnectedClients = std::list<Client<Proto>>;
            struct cache_handle_t final {
                typename ConnectedClients::iterator it;
                int64_t           alive{0};
            };
            using ClientCache = std::deque<cache_handle_t>;

        public:

            template <typename... Args>
            RedisDb(const char *host, int port, Args... args)
                : addr(ipremote(host, port, 0, utils::after(3000)))
            {
                utils::apply_config(config, args...);
            }

            RedisDb()
            {}

            template <typename Opts>
            void configure(const char *host, int port, Opts& opts) {
                addr = ipremote(host, port, 0, utils::after(3000));
                utils::apply_options(Ego.config, opts);
            }

            Client<Proto>& connect(int db = 0) {
                Proto proto;
                auto it = fromCache();
                if (it == Ego.clients.end()) {
                    it = newConnection();
                }

                Client<Proto>& cli = *it;
                // ensure that the server is accepting commands
                if (!cli.ping()) {
                    throw Exception::create("redis - ping Request failed");
                }

                if (db != 0) {
                    idebug("changing database to %d", db);
                    auto resp = cli("SELECT", 1);
                    if (!resp) {
                        throw Exception::create(
                                "redis - changing to selected database '",
                                db, "' failed: ", resp.error());
                    }
                }

                return cli;
            }

            const ServerInfo& getinfo(Client<Proto>& cli, bool refresh = true) {
                if (refresh || !srvinfo.version) {
                    if (!cli.info(srvinfo)) {
                        ierror("retrieving server information failed");
                    }
                }
                return srvinfo;
            }

            ~RedisDb() {
                if (cleaning) {
                    /* unschedule the cleaning coroutine */
                    trace("notifying cleanup routine to exit");
                    !notify;
                }

                trace("cleaning up %lu connections", Ego.cache.size());
                auto it = Ego.cache.begin();

                while (it != Ego.cache.end()) {
                    Ego.clients.erase(it->it);
                    Ego.cache.erase(it);
                    it = Ego.cache.begin();
                }
                Ego.clients.clear();
            }

        private:
            typename ConnectedClients::iterator fromCache() {
                if (!Ego.cache.empty()) {
                    auto handle = Ego.cache.front();
                    Ego.cache.pop_front();
                    return handle.it;
                }
                return Ego.clients.end();
            }

            typename ConnectedClients::iterator newConnection() {
                Proto proto;
                trace("opening redis Connection");
                if (!proto.connect(addr, Ego.config.timeout)) {
                    throw Exception::create("connecting to redis server '",
                                            ipstr(Ego.addr), "' failed: ", errno_s);
                }
                trace("connected to redis server");
                Client<Proto> cli(
                        std::move(proto),
                        Ego.config,
                        std::bind(&RedisDb::returnConnection,this, std::placeholders::_1, std::placeholders::_2));
                auto it = Ego.clients.insert(Ego.clients.end(), std::move(cli));
                it->cacheId =  it;

                if (!config.passwd.empty()) {
                    // authenticate
                    if (!it->auth(config.passwd.c_str())) {
                        throw Exception::create("redis - authorizing client failed");
                    }
                }

                trace("connected to redis server: %s", ipstr(addr));
                return it;
            }

            void returnConnection(typename ConnectedClients::iterator it, bool dctor) {
                cache_handle_t entry{it, -1};
                if (!dctor && (config.keep_alive != 0)) {
                    entry.alive = mnow() + Ego.config.keep_alive;
                    cache.push_back(std::move(entry));
                    if (!Ego.cleaning) {
                        // schedule cleanup routine
                        go(cleanup(Ego));
                    }
                }
                else if (it != clients.end()){
                    // caching not supported, delete client
                    it->closeHandler = nullptr;
                    clients.erase(it);
                }
            }

            static coroutine void cleanup(RedisDb<Proto>& db) {
                bool status;
                int64_t expires = db.config.keep_alive + 5;
                if (db.cache.empty())
                    return;

                db.cleaning = true;
                do {
                    uint8_t status{0};
                    if (db.notify[expires] >> status) {
                        if (status == 1) break;
                    }

                    /* was not forced to exit */
                    auto it = db.cache.begin();
                    /* un-register all expired connections and all that will expire in the
                     * next 500 ms */
                    int64_t t = mnow() + 500;
                    int pruned = 0;
                    ltrace(&db, "starting prune with %ld connections", db.cache.size());
                    while (it != db.cache.end()) {
                        if ((*it).alive <= t) {
                            if (db.isValid(it->it)) {
                                it->it->closeHandler = nullptr;
                                db.clients.erase(it->it);
                            }
                            db.cache.erase(it);
                            it = db.cache.begin();
                        } else {
                            /* there is no point in going forward */
                            break;
                        }

                        if ((++pruned % 100) == 0) {
                            /* avoid hogging the CPU */
                            yield();
                        }
                    }
                    ltrace(&db, "pruned %ld connections", pruned);

                    if (it != db.cache.end()) {
                        /*ensure that this will run after at least 3 second*/
                        expires = std::max((*it).alive - t, (int64_t)3000);
                    }
                } while (!db.cache.empty());

                db.cleaning = false;
            }

            inline bool isValid(typename ConnectedClients::iterator& it) {
                return (it != typename ConnectedClients::iterator{nullptr}) && it != clients.end();
            }

            ConnectedClients     clients;
            ClientCache          cache;
            ipaddr         addr;
            redisdb_config config{1500, ""};
            ServerInfo    srvinfo;
            Channel<uint8_t>  notify{1};
            bool           cleaning{false};
        };

        struct Transaction : LOGGER(REDIS) {
            template <typename... Params>
            bool operator()(const char *cmd, Params... params) {
                if (Ego.in_multi) {
                    Response resp = client.send(cmd, params...);
                    return resp.status();
                }
                iwarn("Executing command '%s' in invalid transaction", cmd);
                return false;
            }

            Response& exec();

            bool discard();

            bool multi();

            Transaction(BaseClient& client);

            virtual ~Transaction();

        private:
            Response             cachedresp;
            BaseClient&          client;
            bool                 in_multi{false};
        };
    }
}
#endif //SUIL_REDIS_HPP
