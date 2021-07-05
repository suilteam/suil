//
// Created by dc on 2019-12-16.
//

#include "../processor.h"

#include "processor.pb.h"

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

    TransactionProcessor::TransactionProcessor(suil::String&& validator)
        : mValidator{validator},
          mDispatcher{mContext},
          mStream{mDispatcher.createStream()},
          mConnMonitor{mContext}
    {}

    TransactionFamily& TransactionProcessor::registerFamily(TransactionFamily::UPtr &&handler)
    {
        auto fn = handler->name();
        idebug("registerHandler adding handler for %s", fn());
        if (Ego.mFamilies.count(fn) != 0) {
            throw Exception::create("Transaction family '", fn, "' already registered");
        }
        auto it = Ego.mFamilies.emplace(fn.dup(), std::move(handler));
        return *(it.first->second.get());
    }

    void TransactionProcessor::registerAll()
    {
        for(const auto& [fn, handler]: Ego.mFamilies) {
            idebug("registerAll - registering handler %s", fn());
            auto versions = handler->versions();
            for(const auto& version: versions) {
                idebug("registerAll - register handler {name: %s, version: %s}", fn(), version());
                sp::TpRegisterRequest req;
                sp::TpRegisterResponse resp;
                setValue(req, &sp::TpRegisterRequest::set_family, fn);
                setValue(req, &sp::TpRegisterRequest::set_version, version);
                for (const auto& ns: handler->namespaces()) {
                    setValue(req, &sp::TpRegisterRequest::add_namespaces, ns);
                }
                auto future = Ego.mStream.sendAsync(msgtype(TP_REGISTER_REQUEST), req);
                future->get(resp, msgtype(TP_REGISTER_RESPONSE));
                if (resp.status() != sp::TpRegisterResponse::OK) {
                    throw Exception::create("failed to register handler {name: ",
                            fn, ", version: ", version, "}");
                }
                idebug("registerAll - handler successfully registered");
            }
        }
    }

    void TransactionProcessor::unRegisterAll()
    {
        sp::TpUnregisterRequest req;
        sp::TpUnregisterResponse resp;

        auto future = Ego.mStream.sendAsync(msgtype(TP_UNREGISTER_REQUEST), req);
        future->get(resp, msgtype(TP_UNREGISTER_REQUEST));

        if (resp.status() != sp::TpUnregisterResponse::OK) {
            ierror("TpUnregisterRequest failed: ", resp.status());
        }
        else {
            idebug("TpUnregisterRequest ok");
        }
    }

    void TransactionProcessor::handleRequest(const suil::Data &msg, const suil::String &cid)
    {
        sp::TpProcessRequest req;
        sp::TpProcessResponse resp;
        try {
            req.ParseFromArray(msg.cdata(), static_cast<int>(msg.size()));
            sp::TransactionHeader* txnHeader{req.release_header()};
            suil::String fn{txnHeader->family_name()};

            auto it = Ego.mFamilies.find(fn);
            if (it != Ego.mFamilies.end()) {
                try {
                    Transaction txn(TransactionHeader{txnHeader}, req.payload(), req.signature());
                    GlobalState gs(mDispatcher.createStream(), String{req.context_id(), true});
                    auto applicator = it->second->transactor(std::move(txn), std::move(gs));
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
        Ego.mStream.respond(sp::Message::TP_PROCESS_RESPONSE, resp, cid);
    }

    void TransactionProcessor::connectionMonitor(suil::sawsdk::TransactionProcessor &Self)
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

    void TransactionProcessor::run() {
        try {
            zmq::Dealer sock(Ego.mContext);
            sock.connect("inproc://request_queue");
            Ego.mDispatcher.connect(Ego.mValidator);

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
}