//
// Created by dc on 2019-12-20.
//

#include "validator.h"

namespace suil::sawsdk::Client {

    ValidatorContext& ValidatorContext::instance() {
        static ValidatorContext context;
        return context;
    }

    ValidatorContext::ValidatorContext()
        : mDispatcher(mContext)
    {}

    bool ValidatorContext::connect(const suil::String &endpoint)
    {
        idebug("connecting to validator %s", endpoint());
        try {
            mDispatcher.connect(endpoint);
        }
        catch (...) {
            auto ex = Exception::fromCurrent();
            ierror("%s", ex.what());
            return false;
        }
        return true;
    }

    void ValidatorContext::disconnect()
    {

    }
}
