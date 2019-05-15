//
// Created by dc on 09/11/18.
//

#ifndef SUIL_TMSP_ABCI_H
#define SUIL_TMSP_ABCI_H

#include <suil/logging.h>
#include <suil/result.h>

#include <suil/tmsp/types.pb.h>

namespace suil::tmsp {

    define_log_tag(TMSP);

    enum Codes : int {
        Ok = 0,
        NotSupported,
        InternalError,
        EncodingError
    };

    struct Application : LOGGER(TMSP) {

        virtual void flush() {
            trace("app::flush not implemented");
        }

        virtual void getInfo(types::ResponseInfo &info) = 0;

        virtual Result setOption(const String &key, const String &value, types::ResponseSetOption &resp) {
            trace("app::setOption not supported");
            return Result{Codes::NotSupported};
        }

        virtual Result deliverTx(const suil::Data &tx, types::ResponseDeliverTx &resp) {
            trace("app::deliverTx not implmented");
            return Result{Codes::Ok};
        }

        virtual Result checkTx(const Data &tx, types::ResponseCheckTx &resp) {
            trace("app::checkTx not implemented");
            return Result{Codes::Ok};
        }

        virtual void commit(types::ResponseCommit &resp) {
            trace("app::commit not implemented");
        }

        virtual Result query(const types::RequestQuery &req, types::ResponseQuery &resp) {
            trace("app::query not implmented");
            return Result{Codes::Ok};
        }

        virtual void initChain(const types::RequestInitChain &req, types::ResponseInitChain &resp) {
            trace("app::initChain not implemented");
        }

        virtual void beginBlock(const types::RequestBeginBlock &req, types::ResponseBeginBlock &resp) {
            trace("app::beginBlock not implemented");
        }

        virtual void endBlock(const types::RequestEndBlock &req, types::ResponseEndBlock &resp) {
            trace("app::endBlock not implemented");
        }
    };

    template<typename Sock, typename L>
    int readProtobufSize(L* l, const char *dbg, Sock &sock, size_t &out) {
        uint8_t  b = {0};
        size_t   size{1};
        int      s{0}, i{0};

        out = 0;
        for (;;i++) {
            if (!sock.receive(&b, size)) {
                // receiving lsz failed
                ldebug(l, "###%s - receiving message size failed: %s", dbg, errno_s);
                return -errno;
            }
            if (b < 0x80) {
                if (i > 9 || (i == 9 && (b > 1))) {
                    return -EOVERFLOW;
                }
                out |= b << s;
                auto x = (int64_t) (out >> 1);
                if (out&1)
                    x ^= x;
                out = x;
                return 0;
            }
            out |= (b&0x7f) << s;
            s += 7;
        }
    }

    template<typename Sock, typename L>
    int writeProtobufSize(L* l, const char *dbg, Sock &sock, int64_t sz) {
        uint8_t  b = {0};
        auto   len = (size_t) (sz << 1);
        if (sz < 0)
            len ^= len;

        size_t size{1};

        for (;len >= 0x80;) {
            b = (uint8_t) ((len &  0xFF) | 0x80);
            if (!sock.send(&b, size)) {
                // receiving lsz failed
                ldebug(l, "###%s - sending message size failed: %s", dbg, errno_s);
                return -errno;
            }
            len >>= 7;
        }
        b = (uint8_t) (len &  0xFF);
        if (!sock.send(&b, size)) {
            // receiving lsz failed
            ldebug(l, "###%s - sending message size failed: %s", dbg, errno_s);
            return -errno;
        }
        return 0;
    }
}
#endif // SUIL_TMSP_ABCI_H