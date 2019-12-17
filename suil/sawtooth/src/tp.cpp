//
// Created by dc on 2019-12-16.
//

#include "gstate.h"
#include "processor.pb.h"

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
          mRespStream{mDispatcher.createStream()}
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

            auto it = Ego.mHandlers.find(fn);
            if (it != Ego.mHandlers.end()) {
                try {
                    Transaction txn(TransactionHeader{txnHeader}, req.release_payload(), req.release_signature());
                    GlobalState gs(new GlobalStateContext(mDispatcher.createStream(), String{req.context_id()}.dup()));
                    auto applicator = it->second->getTransactor(std::move(txn), std::move(gs));
                    try {
                        applicator->apply();
                    }
                    catch (...) {
                        auto ex = Exception::fromCurrent();
                        ierror("Transaction apply error: %s", ex.what());
                        if (ex.Code == Errors::InvalidTransaction) {
                            resp.set_status(sp::TpProcessResponse::INVALID_TRANSACTION);
                        } else {
                            resp.set_status(sp::TpProcessResponse::INTERNAL_ERROR);
                        }
                    }
                }
                catch (...) {
                    auto ex = Exception::fromCurrent();
                    ierror("Transaction apply error: %s", ex.what());
                    resp.set_status(sp::TpProcessResponse::INTERNAL_ERROR);
                }
            }
            else {
                resp.set_status(sp::TpProcessResponse::INVALID_TRANSACTION);
            }
        }
        catch (...) {
            auto ex = Exception::fromCurrent();
            ierror("Transaction process error: %s", ex.what());
            resp.set_status(sp::TpProcessResponse::INTERNAL_ERROR);
        }

        Ego.mRespStream.sendMessage(sp::Message::TP_PROCESS_RESPONSE, resp);
    }

    void TpContext::run() {
        try {
            zmq::Dealer dealer(Ego.mContext);
            dealer.connect("inproc://request_queue");
            Ego.mDispatcher.connect(Ego.mConnString);

            bool isServerConnected{false};
            Ego.mRunning = true;
            while (Ego.mRunning) {
                auto msg = dealer.receive();
                sp::Message validatorMsg;
                validatorMsg.ParseFromArray(msg.data(), static_cast<int>(msg.size()));

                switch (validatorMsg.message_type()) {
                    case sp::Message::TP_PROCESS_REQUEST: {
                        Ego.handleRequest(msg, String{validatorMsg.correlation_id()});
                        break;
                    }

                    case Dispatcher::SERVER_CONNECT_EVENT: {
                        if (!isServerConnected) {
                            idebug("Server connected");
                            Ego.registerAll();
                        }
                        isServerConnected = true;
                        break;
                    }

                    case Dispatcher::SERVER_DISCONNECT_EVENT: {
                        idebug("Server disconnected");
                        isServerConnected = false;
                        break;
                    }
                    default: {
                        idebug("Unknown message in transaction processor");
                        break;
                    }
                }
            }
        }
        catch (...) {
            auto ex = Exception::fromCurrent();
            ierror("Unexpected error while running TP: %s", ex.what());
        }

        idebug("TP done, unregistering");
        Ego.unRegisterAll();
        Ego.mDispatcher.close();
    }

    TransactionProcessor::TransactionProcessor(suil::String &&connString)
        : mContext(new TpContext(std::move(connString)))
    {}

    void TransactionProcessor::registerHandler(TransactionHandler::UPtr &&handler) {
        mContext->registerHandler(std::move(handler));
    }

    void TransactionProcessor::run() {
        mContext->run();
    }

    TransactionProcessor::~TransactionProcessor() {
        if (mContext != nullptr) {
            delete  mContext;
            mContext = nullptr;
        }
    }

}