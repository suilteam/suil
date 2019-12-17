
#include "../sdk.h"
#include "gstate.h"
#include "state_context.pb.h"


namespace sp {
    using sawtooth::protos::Message;
    using sawtooth::protos::TpStateGetRequest;
    using sawtooth::protos::TpStateGetResponse;
    using sawtooth::protos::TpStateSetRequest;
    using sawtooth::protos::TpStateSetResponse;
    using sawtooth::protos::TpStateDeleteRequest;
    using sawtooth::protos::TpStateDeleteResponse;
    using sawtooth::protos::TpEventAddRequest;
    using sawtooth::protos::TpEventAddResponse;
    using sawtooth::protos::TpStateEntry;
    using sawtooth::protos::Event;
    using sawtooth::protos::EventFilter;
    using sawtooth::protos::Event_Attribute;
}

namespace suil::sawsdk {

    GlobalStateContext::GlobalStateContext(Stream &&stream, const suil::String &contextId)
        : mStream{std::move(stream)},
          mContextId{contextId.dup()}
    {}

    GlobalStateContext::GlobalStateContext(GlobalStateContext &&other)
        :  mStream(std::move(other.mStream)),
           mContextId(std::move(other.mContextId))
    {}

    GlobalStateContext& GlobalStateContext::operator=(GlobalStateContext &&other)
    {
        Ego.mStream = std::move(other.mStream);
        Ego.mContextId = std::move(other.mContextId);
        return Ego;
    }

    suil::Data GlobalStateContext::getState(const suil::String &address)
    {
        std::vector<suil::String> addresses = {address.peek()};
        suil::Map<suil::Data> out{};
        Ego.getState(out, addresses);
        if (!out.empty()) {
            return std::move(out.begin()->second);
        }
        return {};

    }

    void GlobalStateContext::getState(Map<suil::Data>& data, const std::vector<suil::String> &addresses)
    {
        data.clear();

        sp::TpStateGetRequest req;
        sp::TpStateGetResponse resp;

        setValue(req, &sp::TpStateGetRequest::set_context_id, Ego.mContextId);
        for(const auto& addr: addresses) {
            setValue(req, &sp::TpStateGetRequest::add_addresses, addr);
        }
        auto future = Ego.mStream.sendMessage(sp::Message::TP_STATE_GET_REQUEST, req);
        future->getMessage(resp, sp::Message::TP_STATE_GET_RESPONSE);

        if (resp.status() == sp::TpStateGetResponse::AUTHORIZATION_ERROR) {
            throw Exception::create("Global state get authorization error - Check transaction inputs");
        }

        if (resp.entries_size() > 0) {
            for (const auto& entry: resp.entries()) {
                data.emplace(String{entry.address(), true}, fromStdString(entry.data()));
            }
        }
    }

    void GlobalStateContext::setState(const suil::String &address, const suil::Data &value)
    {
        std::vector<GlobalState::KeyValue> data = {{address.peek(), value.peek()}};
        Ego.setState(data);
    }

    void GlobalStateContext::setState(const std::vector<GlobalState::KeyValue> &data)
    {
        sp::TpStateSetRequest req;
        sp::TpStateSetResponse resp;

        setValue(req, &sp::TpStateSetRequest::set_context_id, Ego.mContextId);
        for (const auto[first, second]: data) {
            auto& entry = *req.add_entries();
            setValue(entry, &sp::TpStateEntry::set_address, first);
            entry.set_data(second.cdata(), second.size());
        }

        auto future = Ego.mStream.sendMessage(sp::Message::TP_STATE_SET_REQUEST, req);
        future->getMessage(resp, sp::Message::TP_STATE_SET_RESPONSE);
        if (resp.status() == sp::TpStateSetResponse::AUTHORIZATION_ERROR) {
            throw Exception::create("Set global state authorization error - check inputs");
        }
    }

    void GlobalStateContext::deleteState(const suil::String &address)
    {
        std::vector<suil::String> addresses{address.peek()};
        Ego.deleteState(addresses);
    }

    void GlobalStateContext::deleteState(const std::vector<suil::String> &addresses)
    {
        sp::TpStateDeleteRequest req;
        sp::TpStateDeleteResponse resp;

        setValue(req, &sp::TpStateDeleteRequest::set_context_id, Ego.mContextId);
        for (const auto& addr: addresses) {
            setValue(req, &sp::TpStateDeleteRequest::add_addresses, addr);
        }

        auto future = Ego.mStream.sendMessage(sp::Message::TP_STATE_DELETE_REQUEST, req);
        future->getMessage(resp, sp::Message::TP_STATE_DELETE_RESPONSE);

        if (resp.status() == sp::TpStateDeleteResponse::AUTHORIZATION_ERROR) {
            throw Exception::create("global state authorization error - check transaction inputs");
        }
    }

    void GlobalStateContext::addEvent(
            const suil::String &eventType,
            const std::vector<GlobalState::KeyValue> &values,
            const suil::Data &data)
    {
        sp::Event *event{new sp::Event};
        setValue(*event, &sp::Event::set_event_type, eventType);
        for (const auto& [first, second] : values) {
            auto attr = *event->add_attributes();
            setValue(attr, &sp::Event_Attribute::set_key, first);
            setValue(attr, &sp::Event_Attribute::set_value, second);
        }
        event->set_data(data.cdata(), data.size());

        sp::TpEventAddRequest req;
        sp::TpEventAddResponse resp;

        setValue(req, &sp::TpEventAddRequest::set_context_id, Ego.mContextId);
        req.set_allocated_event(event);
        auto future = Ego.mStream.sendMessage(sp::Message::TP_EVENT_ADD_REQUEST, req);
        future->getMessage(resp, sp::Message::TP_EVENT_ADD_RESPONSE);

        if (resp.status() == sp::TpEventAddResponse::ERROR) {
            throw Exception::create("failed to add event {type: ", eventType, "}");
        }
    }

    GlobalState::GlobalState(GlobalStateContext *ctx)
        : mContext(ctx)
    {}

    GlobalState& GlobalState::operator=(suil::sawsdk::GlobalState &&other)
    {
        Ego.mContext = other.mContext;
        other.mContext = nullptr;
        return Ego;
    }

    GlobalState::GlobalState(GlobalState &&other)
        : mContext{other.mContext}
    {
        other.mContext = nullptr;
    }

    GlobalState::~GlobalState() {
        if (mContext) {
            delete mContext;
            mContext = nullptr;
        }
    }

    const suil::Data GlobalState::getState(const suil::String &address) {
        return mContext->getState(address);
    }

    void GlobalState::getState(suil::Map<suil::Data> &data, const std::vector<suil::String> &addresses) {
        return mContext->getState(data, addresses);
    }

    void GlobalState::setState(const suil::String &address, const suil::Data &value) {
        mContext->setState(address, value);
    }

    void GlobalState::setState(const std::vector<KeyValue> &data) {
        mContext->setState(data);
    }

    void GlobalState::deleteState(const suil::String &address) {
        mContext->deleteState(address);
    }

    void GlobalState::deleteState(const std::vector<suil::String> &addresses) {
        mContext->deleteState(addresses);
    }

    void GlobalState::addEvent(
            const suil::String &eventType,
            const std::vector<KeyValue> &values,
            const suil::Data &data)
    {
        mContext->addEvent(eventType, values, data);
    }
}