//
// Created by dc on 2019-12-16.
//

#include "../sdk.h"
#include "transaction.pb.h"

namespace suil::sawsdk {

    TransactionHeader::TransactionHeader(sawtooth::protos::TransactionHeader *proto)
        : mHeader(proto)
    {}

    TransactionHeader::TransactionHeader(suil::sawsdk::TransactionHeader &&other) noexcept
        : mHeader(other.mHeader)
    {
        other.mHeader = nullptr;
    }

    TransactionHeader& TransactionHeader::operator=(suil::sawsdk::TransactionHeader &&other) noexcept
    {
        Ego.mHeader = other.mHeader;
        other.mHeader = nullptr;
        return Ego;
    }

    TransactionHeader::~TransactionHeader() {
        if (mHeader) {
            delete mHeader;
            mHeader = nullptr;
        }
    }

    int TransactionHeader::getCount(TransactionHeader::Field field) {
        switch (field) {
            case TransactionHeader::Field::StringDependencies:
                return mHeader->dependencies_size();
            case Field::Inputs:
                return mHeader->inputs_size();
            case Field::Outputs:
                return mHeader->outputs_size();
            case Field::Nonce:
            case Field::FamilyName:
            case Field::FamilyVersion:
            case Field::PayloadSha512:
            case Field::BatcherPublicKey:
            case Field::SignerPublicKey:
                return 1;
            default:
                return 0;
        }
    }

    suil::Data TransactionHeader::getValue(TransactionHeader::Field field, int index)
    {
        switch (field) {
            case TransactionHeader::Field::StringDependencies:
                return fromStdString(mHeader->dependencies(index));
            case Field::Inputs:
                return fromStdString(mHeader->inputs(index));
            case Field::Outputs:
                return fromStdString(mHeader->outputs(index));
            case Field::Nonce:
                return fromStdString(mHeader->nonce());
            case Field::FamilyName:
                return fromStdString(mHeader->family_name());
            case Field::FamilyVersion:
                return fromStdString(mHeader->family_version());
            case Field::PayloadSha512:
                return fromStdString(mHeader->payload_sha512());
            case Field::BatcherPublicKey:
                return fromStdString(mHeader->batcher_public_key());
            case Field::SignerPublicKey:
                return fromStdString(mHeader->signer_public_key());
            default:
                return suil::Data{};
        }
    }

    TransactionHandler::TransactionHandler(const String &family, const String &ns)
        : mAddressEcoder(ns),
          mFamily(family.dup())
    {
        mNamespaces.push_back(ns.dup());
    }

    const String& TransactionHandler::getFamily() const {
        return mFamily;
    }

    StringVec& TransactionHandler::getNamespaces() {
        return mNamespaces;
    }

    StringVec& TransactionHandler::getVersions() {
        return mVersions;
    }
}