//
// Created by dc on 2019-12-19.
//

#ifndef SUIL_VALIDATOR_H
#define SUIL_VALIDATOR_H

#include "../client.h"
#include "dispatcher.h"

namespace suil::sawsdk::Client {

    struct ValidatorContext final : LOGGER(SAWSDK_DISPATCHER) {
        static ValidatorContext& instance();
        bool connect(const suil::String& endpoint);
        void disconnect();
    private:
        friend struct Validator;
        ValidatorContext();
        zmq::Context mContext;
        Dispatcher   mDispatcher;
    };
}
#endif //SUIL_VALIDATOR_H
