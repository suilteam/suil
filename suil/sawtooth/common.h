//
// Created by Carter Mbotho on 2020-04-08.
//

#ifndef SUIL_COMMON_H
#define SUIL_COMMON_H

#include  <type_traits>

#include <suil/base.h>
#include <suil/utils.h>
#include <suil/logging.h>

#include <suil/sawtooth/protos.h>

#ifdef msgtype
#undef msgtype
#endif
#define msgtype(name) ::sawtooth::protos::Message_MessageType_ ##name

namespace suil::sawsdk {

    define_log_tag(SAWSDK);

    inline suil::Data fromStdString(const std::string& str) {
        return suil::Data{str.data(), str.size(), false};
    }

    template <typename T, typename V>
    inline void setValue(T& to, void(T::*func)(const char*, size_t), const V& data) {
        (to.*func)(reinterpret_cast<const char *>(data.data()), data.size());
    }

    template <typename T, typename V>
    inline void setValue(T& to, void(T::*func)(const void*, size_t), const V& data) {
        (to.*func)(data.data(), data.size());
    }

    template <typename T>
    inline suil::Data protoSerialize(const T& proto) {
        suil::Data data{proto.ByteSizeLong()};
        proto.SerializeToArray(data.data(), static_cast<int>(data.size()));
        return data;
    }

#define protoSet(obj, prop, val) \
        setValue(obj, &std::remove_reference_t<decltype(obj)>::set_##prop, val)
#define protoAdd(obj, prop, val) \
        setValue(obj, &std::remove_reference_t<decltype(obj)>::add_##prop, val)

    using StringVec = std::vector<String>;

    struct Errors final {
        static constexpr int InvalidTransaction = 0xFF00;
        static constexpr int InternalError = 0xFF01;

        template <typename ...Args>
        static Exception invalidTransaction(Args... args) {
            return Exception::create0(InvalidTransaction, args...);
        }

        template <typename ...Args>
        static Exception internalError(Args&... args) {
            return Exception::create0(InternalError, std::forward<Args>(args)...);
        }
    };

    struct TransactionHeader {
        enum Field {
            BatcherPublicKey = 1,
            StringDependencies,
            FamilyName,
            FamilyVersion,
            Inputs,
            Nonce,
            Outputs,
            PayloadSha512,
            SignerPublicKey
        };

        TransactionHeader(sawtooth::protos::TransactionHeader* proto);
        TransactionHeader(TransactionHeader&& other) noexcept;
        TransactionHeader& operator=(TransactionHeader&& other) noexcept ;

        TransactionHeader(const TransactionHeader&) = delete;
        TransactionHeader& operator=(const TransactionHeader&) = delete;

        virtual int getCount(Field field) const;
        virtual suil::Data getValue(Field field, int index = 0) const;

        virtual ~TransactionHeader();

    private:
        sawtooth::protos::TransactionHeader* mHeader{nullptr};
    };

    struct Transaction final {
        Transaction(TransactionHeader&& header, std::string payload, std::string signature)
                : mHeader(std::move(header)),
                  mPayload(std::move(payload)),
                  mSignature(std::move(signature))
        {}

        Transaction(Transaction&& other)
                : mHeader(std::move(other.mHeader)),
                  mPayload(std::move(other.mPayload)),
                  mSignature(std::move(mSignature))
        {}

        Transaction&operator=(Transaction&& other) {
            mHeader = std::move(other.mHeader);
            mPayload = std::move(other.mPayload);
            mSignature = std::move(other.mSignature);
            return Ego;
        }

        Transaction(const Transaction&) = delete;
        Transaction&operator=(const Transaction&) = delete;

        [[nodiscard]] const TransactionHeader& header() const { return mHeader; }

        [[nodiscard]] suil::Data payload() const {
            return suil::Data{mPayload.data(), mPayload.size(), false};
        }

        [[nodiscard]] suil::Data signature() const {
            return suil::Data{mSignature.data(), mSignature.size(), false};
        }

    private:
        TransactionHeader mHeader;
        std::string mPayload{};
        std::string mSignature{};
    };
}

#endif //SUIL_COMMON_H
