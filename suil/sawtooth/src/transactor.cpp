//
// Created by Carter Mbotho on 2020-04-08.
//

#include "../transactor.h"

namespace suil::sawsdk {

    TransactionFamily::TransactionFamily(const String &family, const String &ns)
        : mFamily(family.dup())
    {
        mNamespaces.push_back(ns.dup());
    }

    const String& TransactionFamily::name() const {
        return mFamily;
    }

    StringVec& TransactionFamily::namespaces() {
        return mNamespaces;
    }

    StringVec& TransactionFamily::versions() {
        return mVersions;
    }

}