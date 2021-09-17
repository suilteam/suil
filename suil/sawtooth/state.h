//
// Created by Carter Mbotho on 2020-04-08.
//

#ifndef SUIL_STATE_H
#define SUIL_STATE_H

#include <suil/sawtooth/stream.h>

namespace suil::sawsdk {

    struct GlobalState final : LOGGER(SAWSDK) {
        using KeyValue = std::tuple<String, Data>;
        sptr(GlobalState);
        
        GlobalState(Stream&& stream, const String& contextId);

        GlobalState(GlobalState&& other);
        GlobalState&operator=(GlobalState&& other);

        GlobalState(const GlobalState&) = delete;
        GlobalState&operator=(const GlobalState&) = delete;

        Data getState(const String& address);

        void getState(Map<Data>& data, const std::vector<String>& addresses);

        void setState(const String& address, const Data& value);

        void setState(const std::vector<KeyValue>& data);

        void deleteState(const String& address);

        void deleteState(const std::vector<String>& addresses) ;

        void addEvent(
                const String& eventType,
                const std::vector<KeyValue>& values,
                const Data& data);

    private:
        Stream  mStream;
        String  mContextId;
    };
}
#endif //SUIL_STATE_H
