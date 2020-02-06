//
// Created by dc on 2019-12-31.
//

#include "../sdk.h"

namespace suil::sawsdk {

    void AddressEncoder::isValidAddr(const suil::String& addr) {
        if (addr.size() != ADDRESS_LENGTH) {
            throw Exception::create("Address size is invalid, {Expected: ",
                                    ADDRESS_LENGTH, ", Got: ", addr.size(), "}");
        }
        else if (!utils::isHexStr(addr)) {
            throw Exception::create("Address must be lowercase hexadecimal ONLY");
        }
    }

    void AddressEncoder::isNamespaceValid(const suil::String& ns) {
        if (ns.size() != NS_PREFIX_LENGTH) {
            throw Exception::create("Namespace size is invalid, {Expected: ",
                                    NS_PREFIX_LENGTH, ", Got: ", ns.size(), "}");
        }
        else if (!utils::isHexStr(ns)) {
            throw Exception::create("Namespace prefix must be lowercase hexadecimal ONLY");
        }
    }

    AddressEncoder::AddressEncoder(const suil::String &ns)
        : mNamespace(ns.dup())
    {}

    AddressEncoder::AddressEncoder(suil::sawsdk::AddressEncoder &&other) noexcept
        : mNamespace(std::move(other.mNamespace)),
          mPrefix(std::move(other.mPrefix))
    {}

    AddressEncoder& AddressEncoder::operator=(suil::sawsdk::AddressEncoder &&other) noexcept
    {
        Ego.mNamespace = std::move(other.mNamespace);
        Ego.mPrefix = std::move(other.mPrefix);
        return Ego;
    }

    suil::String AddressEncoder::mapNamespace(const suil::String &ns) const
    {
        return utils::SHA512(ns).substr(0, NS_PREFIX_LENGTH, false);
    }

    suil::String AddressEncoder::mapKey(const suil::String &key)
    {
        return utils::SHA512(key).substr(NS_PREFIX_LENGTH, ADDRESS_LENGTH-NS_PREFIX_LENGTH, false);
    }

    const suil::String& AddressEncoder::getPrefix()
    {
        if (Ego.mPrefix.empty()) {
            Ego.mPrefix = Ego.mapNamespace(Ego.mNamespace);
            isNamespaceValid(Ego.mPrefix);
        }
        return Ego.mPrefix;
    }

    suil::String AddressEncoder::operator()(const suil::String &key)
    {
        auto addr = utils::catstr(Ego.getPrefix(), Ego.mapKey(key));
        isValidAddr(addr);
        return addr;
    }
}
