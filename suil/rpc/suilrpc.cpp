//
// Created by dc on 27/12/18.
//

#include "suilrpc.h"

namespace suil::rpc {

    SuilRpcHandler::SuilRpcHandler()
    {
        // register extension methods
        Ego.rpcMeta.version = String(SUIL_VERSION_STRING).dup();
        Ego.appendExtensionMethod("rpc_Version");
        Ego.appendExtensionMethod("rpc_enableProtoUseSize");
    }

    int SuilRpcHandler::appendExtensionMethod(suil::String &&method)
    {
        auto& extensions = Ego.rpcMeta.extensions;
        int id = 0;
        for (const auto& ext: extensions) {
            id = ext.id;
            if (ext.name == method)
                return id;
        }
        extensions.emplace_back(--id, std::move(method));
        return id;
    }

    void SuilRpcHandler::appendMethod(int id, suil::String &&method) {
        Ego.rpcMeta.methods.emplace_back(id, std::move(method));
    }

    Result SuilRpcHandler::extensionMethods(
            suil::Breadboard &results, int method, suil::Breadboard &params, int id)
    {
        switch (method) {
            case -1: {
                results << SUIL_VERSION_STRING;
                return Result(0);
            }

            default: {
                // method nto found
                Result res(SRPC_METHOD_NOT_FOUND);
                res << "extension method with id=" << method << " does not exist";
                return res;
            }
        }
    }

    void SuilRpcServerConnection::operator()(suil::SocketAdaptor &sock, suil::rpc::SuilRpcHandler *h)
    {
        Ego.handler = h;
        try {
            OBuffer ob{1024};
            Ego.protoUseSize = true;
            do {
                ob.reset(1024, true);
                if (!Ego.receiveRaw(sock, ob))
                    break;

                auto resp = handleRequest(ob);
                if (!Ego.sendRaw(sock, resp))
                    break;

            } while (sock.isopen());
        }
        catch (...) {
            /* Unhandled JSONRPC error */
            ierror("un handled JSON RPC processing error: %s", Exception::fromCurrent().what());
            sock.close();
        }
    }

    suil::Data SuilRpcServerConnection::handleRequest(const suil::OBuffer &req)
    {
        suil::Heapboard hb(req.cdata());
        suil::Heapboard hbOut(1024);
        SuilRpcResponse rpcResponse;
        Result res(0);
        rpcResponse.error.code = 0;

        try {
            SuilRpcRequest rpcRequest;
            hb >> rpcRequest;
            suil::Heapboard bb(rpcRequest.params);
            itrace("handling request {id=%d, method=%d}", rpcRequest.id, rpcRequest.method);
            rpcResponse.id = rpcResponse.id;
            if (rpcRequest.method <= 0) {
                // methods with negative indices are system
                res = Ego.handleExtension(hbOut, rpcRequest.method, bb, rpcRequest.id);
            }
            else {
                // trust on the handler to have implemented the method
                res = (*handler)(hbOut, rpcRequest.method, bb, rpcRequest.id);
            }
        }
        catch (...) {
            // handling request or parsing request failed
            res(SRPC_INTERNAL_ERROR)
                << Exception::fromCurrent().what();
        }

        if (!res.Ok()) {
            // there is an error
            rpcResponse.error.code = res.Code;
            rpcResponse.error.message = "API ERROR";
            rpcResponse.error.data = String(res);
        }
        else {
            // set the data
            rpcResponse.data = hbOut.release();
        }

        suil::Heapboard resp(rpcResponse.data.size()+128);
        resp << rpcResponse;
        return resp.release();
    }

    Result SuilRpcServerConnection::handleExtension(
            suil::Breadboard &results, int method, suil::Breadboard &req, int id)
    {
        if (method == 0) {
            // reserved specifically for querying meta data
            results << Ego.handler->getMeta();
            return Result(0);
        }
        else {
            // any other extension method
            return handler->extensionMethods(results, method, req, id);
        }
    }

    bool __SuilRpcClient::connect(suil::String &&host, int port)
    {
        idebug("SUIL RPC client connecting to server %s:%d", host(), port);
        ipaddr addr = iplocal(host(), port, 0);
        if (!sock.connect(addr, 1500)) {
            /* failed to connect to given host */
            ierror("SUIL RPC connection to %s:%d failed: %s", host(), port, errno_s);
            return false;
        }

        Ego.protoUseSize = true;

        if (Ego.getMeta().version.empty())
            iwarn("getting service meta data failed");

        auto verString = Ego.getVersion();
        if (!verString) {
            /* communication with service failed */
            ierror("SUIL RPC client failed to communicate with service");
            sock.close();
            return false;
        }

        idebug("Successfully connected to service, running suil version %s", verString());
        return true;
    }

    String __SuilRpcClient::getVersion()
    {
        return Ego.call<String>("rpc_Version");
    }

    const SuilRpcMeta& __SuilRpcClient::getMeta()
    {
        if (Ego.rpcMeta.version.empty()) {
            // not fetched yet
            Ego.rpcMeta = Ego.call<SuilRpcMeta >("");
            for (auto& m: rpcMeta.methods) {
                // build map
                Ego.apiMethods.emplace(m.name.peek(), m.id);
            }
            for (auto& m: rpcMeta.extensions) {
                // build map
                Ego.extMethods.emplace(m.name.peek(), m.id);
            }
        }
        return Ego.rpcMeta;
    }

    SuilRpcResponse __SuilRpcClient::call(suil::Heapboard& res, suil::String &&method, suil::Breadboard &params)
    {
        int methodId{0};
        bool found{true};
        if (!method.empty()) {
            // empty method retrieves meta
            if (strstr(method(), "rpc_") != nullptr) {
                // looking for an extension method
                auto it = Ego.extMethods.find(method);
                if (it != Ego.extMethods.end()) {
                    methodId = it->second;
                }
                else {
                    found = false;
                }
            } else {
                // looking for an API method
                auto it = Ego.apiMethods.find(method);
                if (it != Ego.apiMethods.end()) {
                    methodId = it->second;
                }
                else {
                    found = false;
                }
            }
        }

        if (!found) {
            // requested method does not exist
            throw Exception::create("requested method '", method(), "' does not exist");
        }

        SuilRpcRequest rpcRequest;
        rpcRequest.id = Ego.idGenerator++;
        rpcRequest.method = methodId;
        rpcRequest.params = params.raw();

        // send request to client
        suil::Heapboard hb(params.size()+128);
        hb << rpcRequest;
        if (!Ego.sendRaw(Ego.sock, hb.raw())) {
            // sending request failed
            throw Exception::create("sending request to server failed: ", errno_s);
        }

        OBuffer ob;
        if (!Ego.receiveRaw(sock, ob)) {
            // receiving output failed
            throw Exception::create("receiving response failed: ", errno_s);
        }

        auto resSize = ob.size();
        res = suil::Heapboard((const uint8_t *)ob.release(), resSize, true);
        SuilRpcResponse resp;
        res >> resp;
        if (resp.error.code) {
            // request failed
            throw Exception::create(0, resp.error.message, " - ", resp.error.data);
        }

        return resp;
    }
}
