//
// Created by Carter Mbotho on 2020-04-08.
//

#ifndef SUIL_ADDRESS_H
#define SUIL_ADDRESS_H

#include <suil/zstring.h>

namespace suil::sawsdk {

    struct AddressEncoder final {
        static constexpr size_t ADDRESS_LENGTH = 70;
        static constexpr size_t NS_PREFIX_LENGTH = 6;

        AddressEncoder(const String& ns);
        AddressEncoder(AddressEncoder&& other) noexcept ;
        AddressEncoder&operator=(AddressEncoder&& other) noexcept ;

        AddressEncoder(const AddressEncoder&) = delete;
        AddressEncoder&operator=(const AddressEncoder&) = delete;

        String operator()(const String& key);

        const String& prefix();

        static void isValidAddr(const String& addr);

        static void isNamespaceValid(const String& ns);

    private:

        String mapNamespace(const String& key) const;
        String mapKey(const String& key);

    private:
        String mNamespace{};
        String mPrefix{};
    };
}

#endif //SUIL_ADDRESS_H
