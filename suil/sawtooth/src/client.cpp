
#include "../sdk.h"
#include "../client.h"
#include "dispatcher.h"

namespace sp {
    using namespace sawtooth::protos;
}

namespace suil::sawsdk::Client {

    Signer::Signer(crypto::ECKey&& key)
        : mKey(std::move(key))
    {}

    Signer:: Signer(Signer&& other) noexcept
        : mKey(std::move(other.mKey))
    {}

    Signer& Signer::operator=(Signer& other) noexcept
    {
        Ego.mKey = std::move(other.mKey);
        return Ego;
    }

    Signer Signer::load(const suil::String& privKey)
    {
        auto priv = crypto::PrivateKey::fromString(privKey);
        if (!priv.nil()) {
            return Signer::load(priv);
        }
        else {
            return Signer{{}};
        }
    }

    Signer Signer::load(const crypto::PrivateKey& privKey)
    {
        return Signer{crypto::ECKey::fromKey(privKey)};
    }

    void Signer::get(crypto::PrivateKey& out) const
    {
        if (Ego.isValid()) {
            out.copyfrom(Ego.getPrivateKey());
        }
    }

    void Signer::get(crypto::PublicKey& out) const {
        if (Ego.isValid()) {
            out.copyfrom(Ego.getPublicKey());
        }
    }

    const crypto::PrivateKey& Signer::getPrivateKey() const {
        return Ego.mKey.getPrivateKey();
    }

    const crypto::PublicKey& Signer::getPublicKey() const {
        return Ego.mKey.getPublicKey();
    }

    suil::String Signer::sign(const void* data, size_t len) const {
        crypto::ECDSASignature signature;
        if (crypto::ECDSASign(signature, Ego.getPrivateKey(), data, len)) {
            ierror("Signer::sign signing message failed");
            return signature.toCompactForm();
        }
        return {};
    }

    bool Signer::verify(const void* data, size_t len, const suil::String& sig, const suil::String& publicKey)
    {
        LOGGER(SAWSDK_CLIENT) lt;
        auto signature = crypto::ECDSASignature::fromCompactForm(sig);
        if (signature.nil()) {
            lerror(&lt, "Signer::verify loading signature failed");
            return false;
        }
        auto pubKey = crypto::PublicKey::fromString(publicKey);
        if (pubKey.nil()) {
            lerror(&lt, "Signer::verify loading public key failed");
            return false;
        }
        return crypto::ECDSAVerify(data, len, signature, pubKey);
    }

    Transaction::Transaction()
        : mSelf(new sawtooth::protos::Transaction)
    {}

    sp::Transaction& Transaction::get() {
        return *mSelf;
    }

    sawtooth::protos::Transaction& Transaction::operator*() {
        return *mSelf;
    }

    sawtooth::protos::Transaction* Transaction::operator->() {
        return Ego.mSelf.get();
    }

    const sawtooth::protos::Transaction& Transaction::operator*() const {
        return *mSelf;
    }

    const sawtooth::protos::Transaction* Transaction::operator->() const {
        return Ego.mSelf.get();
    }

    Batch::Batch()
        : mSelf(new sp::Batch)
    {}

    sawtooth::protos::Batch& Batch::get() {
        return *mSelf;
    }

    sawtooth::protos::Batch* Batch::operator->() {
        return Ego.mSelf.get();
    }

    sawtooth::protos::Batch& Batch::operator*() {
        return *mSelf;
    }

    const sawtooth::protos::Batch* Batch::operator->() const {
        return Ego.mSelf.get();
    }

    const sawtooth::protos::Batch& Batch::operator*() const {
        return *mSelf;
    }

    Encoder::Encoder(const suil::String& family, const suil::String& familyVersion, const suil::String& privateKey)
        : mSigner{Signer::load(privateKey)},
          mFamily(family.dup()),
          mFamilyVersion(familyVersion.dup()),
          mBatcherPublicKey(mSigner.getPublicKey().toString()),
          mSignerPublicKey(mSigner.getPublicKey().toString())
    {}

    Transaction Encoder::operator()(const suil::Data& payload, const Inputs& inputs, const Outputs& outputs) const
    {
        crypto::SHA512Digest sha512;
        crypto::SHA512(sha512, payload);

        sp::TransactionHeader header;
        protoSet(header, family_name, Ego.mFamily);
        protoSet(header, family_version, Ego.mFamilyVersion);
        protoSet(header, signer_public_key, Ego.mSignerPublicKey);
        protoSet(header, batcher_public_key, Ego.mBatcherPublicKey);
        protoSet(header, nonce, utils::uuidstr());
        protoSet(header, payload_sha512, sha512.toString());
        for (const auto& input: inputs) {
            protoAdd(header, inputs, input);
        }
        for (const auto& output: outputs) {
            protoAdd(header, outputs, output);
        }
        auto headerBytes = protoSerialize(header);
        auto signature = Ego.mSigner.sign(headerBytes);

        Transaction txn;
        protoSet(*txn, payload, payload);
        protoSet(*txn, header, headerBytes);
        protoSet(*txn, header_signature, signature);

        return txn;
    }

    Batch Encoder::operator()(const std::vector<Transaction> &txns) const
    {
        sp::BatchHeader header;
        Batch batch;
        for (const auto& txn: txns) {
            protoAdd(header, transaction_ids, txn->header_signature());
            auto it = batch->add_transactions();
            it->CopyFrom(*txn);
        }
        protoSet(header, signer_public_key, Ego.mSigner.getPublicKey().toString());

        auto headerBytes = protoSerialize(header);
        auto signature = mSigner.sign(headerBytes);

        protoSet(*batch, header, headerBytes);
        protoSet(*batch, header_signature, signature);

        return batch;
    }

    suil::Data Encoder::operator()(const std::vector<Batch> &batches) const {
        sp::BatchList list;
        for (const auto& batch: batches) {
            list.add_batches()->CopyFrom(*batch);
        }
        return protoSerialize(list);
    }

    suil::Data Encoder::encode(const suil::Data &payload, const Inputs &inputs, const Outputs &outputs)
    {
        auto txn = Ego(payload, inputs, outputs);
        return Ego(Ego(txn));
    }
}