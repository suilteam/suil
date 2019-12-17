//
// Created by dc on 2019-12-16.
//

#ifndef SUIL_GSTATE_H
#define SUIL_GSTATE_H

#include "../sdk.h"
#include "stream.h"

namespace suil::sawsdk {

    struct GlobalStateContext {

        GlobalStateContext(Stream&& stream, const suil::String& contextId);

        GlobalStateContext(GlobalStateContext&& other);
        GlobalStateContext&operator=(GlobalStateContext&& other);

        GlobalStateContext(const GlobalStateContext&) = delete;
        GlobalStateContext&operator=(const GlobalStateContext&) = delete;

        suil::Data getState(const suil::String& address);

        void getState(Map<suil::Data>& data, const std::vector<suil::String>& addresses);

        void setState(const suil::String& address, const suil::Data& value);

        void setState(const std::vector<GlobalState::KeyValue>& data);

        void deleteState(const suil::String& address);

        void deleteState(const std::vector<suil::String>& addresses) ;

        void addEvent(
                const suil::String& eventType,
                const std::vector<GlobalState::KeyValue>& values,
                const suil::Data& data);

    private:
        Stream  mStream;
        suil::String  mContextId;
    };

}
#endif //SUIL_GSTATE_H
