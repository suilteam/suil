//
// Created by dc on 2019-12-16.
//

#include "processor.pb.h"
#include "transaction.pb.h"

#include "tp.h"

namespace sp {
    using sawtooth::protos::Message;
    using sawtooth::protos::TransactionHeader;
    using sawtooth::protos::TpRegisterRequest;
    using sawtooth::protos::TpRegisterResponse;
    using sawtooth::protos::TpUnregisterRequest;
    using sawtooth::protos::TpUnregisterResponse;
    using sawtooth::protos::TpProcessResponse;
    using sawtooth::protos::TpProcessRequest;
}

namespace suil::sawsdk {

    TpContext::TpContext(suil::String &&connString)
        : mConnString(connString.dup()),
          mDispatcher{mContext},
          mRespStream{mContext, mDispatcher.mOnAirMessages}
    {}

    void TpContext::registerHandler(sawsdk::TransactionHandler::UPtr &&handler)
    {
        auto fn = handler->getFamilyName();
        idebug("registerHandler adding handler for %s", fn());
        Ego.mHandlers[fn.peek()] = std::move(handler);
    }

    void TpContext::registerAll()
    {
        for(const auto& [fn, handler]: Ego.mHandlers) {
            idebug("registerAll - registering handler %s", fn());
            auto versions = handler->getVersions();
            for(const auto& version: versions) {
                idebug("registerAll - register handler {name: %s, version: %s}", fn(), version());
                sp::TpRegisterRequest req;
                sp::TpRegisterResponse resp;
                setValue(req, &sp::TpRegisterRequest::set_family, fn);
                setValue(req, &sp::TpRegisterRequest::set_version, version);
                for (const auto& ns: handler->getNamespaces()) {
                    setValue(req, &sp::TpRegisterRequest::add_namespaces, ns);
                }
                auto future = Ego.mRespStream.sendMessage(sp::Message::TP_REGISTER_REQUEST, req);
                future->getMessage(resp, sp::Message::TP_REGISTER_RESPONSE);
                if (resp.status() != sp::TpRegisterResponse::OK) {
                    throw Exception::create("failed to register handler {name: ",
                            fn, ", version: ", version, "}");
                }
            }
        }
    }

    void TpContext::unRegisterAll()
    {
        sp::TpUnregisterRequest req;
        sp::TpUnregisterResponse resp;

        auto future = Ego.mRespStream.sendMessage(sp::Message::TP_UNREGISTER_REQUEST, req);
        future->getMessage(resp, sp::Message::TP_UNREGISTER_REQUEST);

        if (resp.status() != sp::TpUnregisterResponse::OK) {
            ierror("TpUnregisterRequest failed: ", resp.status());
        }
        else {
            idebug("TpUnregisterRequest ok");
        }
    }

    void TpContext::handleRequest(const suil::Data &msg, const suil::String &cid)
    {
        sp::TpProcessRequest req;
        sp::TpProcessResponse resp;
        try {
            req.ParseFromArray(msg.cdata(), static_cast<int>(msg.size()));
            sp::TransactionHeader* txnHeader{req.release_header()};
            suil::String fn{txnHeader->family_name()};
            Transaction txn(TransactionHeader{txnHeader}, req.release_payload(), req.release_signature());
        }
        catch (suil::Exception& ex) {

        }
    }

}