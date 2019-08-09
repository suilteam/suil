//
// Created by dc on 2019-08-09.
//

#ifndef SUIL_RESOURCECACHE_H
#define SUIL_RESOURCECACHE_H

#include <suil/channel.h>
#include <suil/logging.h>

#include <list>
#include <deque>

namespace suil {

    template <typename Resource>
    struct Cacheable {
        using CacheId = typename std::list<Resource>;
        using CloseHandler = std::function<void(CacheId, bool)>;

        Cacheable(CacheId id, CloseHandler closeHandler = nullptr)
            : id(id),
              closeHandler(closeHandler)
        {}

        void close() {
            if (closeHandler != nullptr && id != CacheId{nullptr}) {
            }
        }

    private:
        CloseHandler  closeHandler;
        CacheId       id{nullptr};
    };

    template <typename Resource>
    struct ResourceCache {
        virtual ~ResourceCache() {
            if (cleaning) {
                /* unschedule the cleaning coroutine */
                strace("notifying cleanup routine to exit");
                !notify;
            }

            strace("cleaning up %lu connections", Ego.cache.size());
            auto it = Ego.cache.begin();

            while (it != Ego.cache.end()) {
                Ego.inflight.erase(it->it);
                Ego.cache.erase(it);
                it = Ego.cache.begin();
            }
            Ego.inflight.clear();
        }

    protected:
        using InflightResources = std::list<Resource>;
        using CacheId = typename InflightResources::iterator;
        struct cache_handle_t final {
            CacheId         id;
            int64_t         alive;
        };
        using CacheEntries = std::deque<cache_handle_t>;

    protected:
        CacheId fromCache() {
            if (!Ego.cache.empty()) {
                auto handle = cache.front();
                cache.pop_front();
                return handle.id;
            }
            return Ego.inflight.end();
        }

        CacheId addResource(Resource&& resource) {
            resource.onClose(std::bind(&ResourceCache::returnResource,
                    this, std::placeholders::_1, std::placeholders::_2));
            auto id = Ego.inflight.insert(Ego.inflight.back(), std::move(resource));
            return id;
        }

        void returnResource(CacheId  id, bool dctor) {
            cache_handle_t entry{id, -1};
            if (!dctor && (keepAlive != 0)) {
                entry.alive = mnow() + keepAlive;
                cache.push_back(std::move(entry));
                if (!Ego.cleaning) {
                    // schedule cleanup routine
                    go(cleanup(Ego));
                }
            }
            else if (id != Ego.inflight.end()){
                // caching not supported, delete client
                id->closeHandler = nullptr;
                Ego.inflight.erase(id);
            }
        }

        static coroutine void cleanup(ResourceCache<Resource>& rc) {
            bool status;
            int64_t expires = rc.keepAlive + 5;
            if (rc.cache.empty())
                return;

            rc.cleaning = true;
            do {
                uint8_t status{0};
                if (rc.notify[expires] >> status) {
                    if (status == 1) break;
                }

                /* was not forced to exit */
                auto it = rc.cache.begin();
                /* un-register all expired connections and all that will expire in the
                 * next 500 ms */
                int64_t t = mnow() + 500;
                int pruned = 0;
                strace("starting prune with %ld connections", rc.cache.size());
                while (it != rc.cache.end()) {
                    if ((*it).alive <= t) {
                        if (rc.isValid(it->id)) {
                            it->id->onClose(nullptr);
                            rc.inflight.erase(it->it);
                        }
                        rc.cache.erase(it);
                        it = rc.cache.begin();
                    } else {
                        /* there is no point in going forward */
                        break;
                    }

                    if ((++pruned % 100) == 0) {
                        /* avoid hogging the CPU */
                        yield();
                    }
                }
                strace("pruned %ld connections", pruned);

                if (it != rc.cache.end()) {
                    /*ensure that this will run after at least 3 second*/
                    expires = std::max((*it).alive - t, (int64_t)3000);
                }
            } while (!rc.cache.empty());

            rc.cleaning = false;
        }

        inline bool isValid(CacheId& it) {
            return (it != CacheId{nullptr}) && it != Ego.inflight.end();
        }

        InflightResources   inflight;
        CacheEntries        cache;
        Channel<uint8_t>    notify{1};
        bool                cleaning{false};
        int64_t             keepAlive{0};
    };

}

#endif //SUIL_RESOURCECACHE_H
