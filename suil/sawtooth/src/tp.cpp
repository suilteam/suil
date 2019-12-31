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
          mRespStream{mDispatcher.createStream()},
          mConnMonitor{mContext}
    {}

    void TpContext::registerHandler(sawsdk::TransactionHandler::UPtr &&handler)
    {
        auto fn = handler->getFamily();
        idebug("registerHandler adding handler for %s", fn());
        Ego.mHandlers[fn.dup()] = std::move(handler);
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
                auto future = Ego.mRespStream.asyncSend(sp::Message::TP_REGISTER_REQUEST, req);
                future->getMessage(resp, sp::Message::TP_REGISTER_RESPONSE);
                if (resp.status() != sp::TpRegisterResponse::OK) {
                    throw Exception::create("failed to register handler {name: ",
                            fn, ", version: ", version, "}");
                }
                idebug("registerAll - handler successfully registered");
            }
        }
    }

    void TpContext::unRegisterAll()
    {
        sp::TpUnregisterRequest req;
        sp::TpUnregisterResponse resp;

        auto future = Ego.mRespStream.asyncSend(sp::Message::TP_UNREGISTER_REQUEST, req);
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
                    Transaction txn(TransactionHeader{txnHeader}, req.payload(), req.signature());
                    GlobalState gs(new GlobalStateContext(mDispatcher.createStream(), String{req.context_id()}.dup()));
                    auto applicator = it->second->getProcessor(std::move(txn), std::move(gs));
                    try {
                        applicator->apply();
                        resp.set_status(sp::TpProcessResponse::OK);
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
        Ego.mRespStream.sendResponse(sp::Message::TP_PROCESS_RESPONSE, resp, cid);
    }

    void TpContext::connectionMonitor(suil::sawsdk::TpContext &Self)
    {
        ldebug(&Self, "Starting connectionMonitor coroutine");
        Self.mConnMonitor.connect(Dispatcher::SERVER_MONITOR_ENDPOINT);
        while (Self.mRunning) {
            auto msg = Self.mConnMonitor.receive();
            if (msg.empty()) {
                lwarn(&Self, "connectionMonitor received an empty message, ignoring");
                continue;
            }

            auto events = *reinterpret_cast<uint16_t *>(msg.data());
            ldebug(&Self, "connectionMonitor received events %02X", events);
            if (events & ZMQ_EVENT_CONNECTED) {
                ldebug(&Self, "Server connected event");
                if (!Self.mIsSeverConnected) {
                    Self.registerAll();
                }
                Self.mIsSeverConnected = true;
            }
            else if (events & ZMQ_EVENT_DISCONNECTED) {
                ldebug(&Self, "Server disconnected event");
                Self.mIsSeverConnected = false;
            }
            else {
                ldebug(&Self, "connectionMonitor ignoring connection events %02X", events);
            }
        }
        ldebug(&Self, "connectionMonitor coroutine exiting");
        Self.mConnMonitor.close();
    }

    void TpContext::run() {
        try {
            zmq::Dealer sock(Ego.mContext);
            sock.connect("inproc://request_queue");
            Ego.mDispatcher.connect(Ego.mConnString);

            Ego.mRunning = true;
            go(connectionMonitor(Ego));

            bool isServerConnected{false};

            while (Ego.mRunning) {
                auto msg = sock.receive();
                sp::Message validatorMsg;
                validatorMsg.ParseFromArray(msg.data(), static_cast<int>(msg.size()));

                switch (validatorMsg.message_type()) {
                    case sp::Message::TP_PROCESS_REQUEST: {

                        Ego.handleRequest(fromStdString(validatorMsg.content()), String{validatorMsg.correlation_id()});
                        break;
                    }
                    default: {
                        idebug("Unknown message in transaction processor: %08X", validatorMsg.message_type());
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
        Ego.mDispatcher.exit();
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