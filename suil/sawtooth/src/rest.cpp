//
// Created by dc on 2019-12-31.
//

#include "../client.h"

namespace suil::sawsdk::Client {

    const char* HttpRest::BATCHES_RESOURCE{"/batches"};
    const char* HttpRest::STATE_RESOURCE{"/state"};

    HttpRest::HttpRest(
            const suil::String &url,
            const suil::String &family,
            const suil::String &familyVersion,
            const suil::String &privateKey, int port)
        : mEncoder{family, familyVersion.dup(), privateKey.dup()},
          mAddressEncoder{family},
          mSession(http::client::load(url(), port))
    {}

    bool HttpRest::asyncBatches(const suil::Data &payload, const StringVec& inputs, const StringVec& outputs)
    {
        std::vector<Batch> batches;
        batches.push_back(Ego.mEncoder.encode(payload, inputs, outputs));
        return Ego.asyncBatches(batches);
    }

    bool HttpRest::asyncBatches(const std::vector<Batch> &batches)
    {
        auto resp = http::client::post(mSession, BATCHES_RESOURCE, [batches](http::client::Request& req) {
            Encoder::encode(req.buffer("application/octet-stream"), batches);
            return true;
        });

        auto body = resp.getbody();
        if (resp.status() == http::Status::ACCEPTED) {
            auto obj = json::Object::decode(body);
            auto link = (String) obj("link");
            sdebug("Transaction created: %s", link());
            return true;
        }
        else {
            serror("%s", body());
        }
    }

    suil::Data HttpRest::getState(const suil::String &key)
    {
        auto resource = utils::catstr(STATE_RESOURCE, "/", mAddressEncoder(key));
        auto resp = http::client::get(mSession, resource());

        auto body = resp.getbody();
        if (resp.status() == http::Status::OK) {
            auto res = json::Object::decode(body);
            auto data = (String) res("data");
            OBuffer ob;
            utils::base64::decode(ob, (const uint8_t *)data.data(), data.size());
            return suil::Data(ob.release(), ob.size(), true);
        }
        else {
            serror("%s", body());
            return {};
        }
    }

    suil::String HttpRest::getPrefix()
    {
        return Ego.mAddressEncoder.getPrefix().peek();
    }
}
