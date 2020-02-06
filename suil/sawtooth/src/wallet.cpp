//
// Created by dc on 2020-01-15.
//

#include "../client.h"

namespace suil::sawsdk::Client {

    static const String EMPTY{nullptr};

    Wallet::Wallet(String&& path, String&& secret)
        : mPath(std::move(path)),
          mSecret(std::move(secret))
    {}

    Wallet::~Wallet() {
        Ego.save();
    }

    Wallet Wallet::open(const String &path, const String& secret)
    {
        if (!utils::fs::exists(path())) {
            throw Exception::create("Wallet '", path, "' not found");
        }
        auto encrypted = utils::fs::readall(path());
        if (encrypted.empty()) {
            throw Exception::create("Wallet '", path, "' does not contain a valid key");
        }

        auto decrypted = utils::AES_Decrypt(secret, encrypted);
        if (decrypted.empty()) {
            throw Exception::create("Unlocking wallet failed, check wallet secret");
        }
        try {
            Wallet w(path.dup(), secret.dup());
            suil::Heapboard hb(decrypted);
            hb >> w.mData;
            return std::move(w);
        }
        catch (...) {
            throw Exception::create("Invalid wallet file contents");
        }
    }

    Wallet Wallet::create(const String& path, const String& secret)
    {
        if (utils::fs::exists(path())) {
            throw Exception::create("Wallet '", path, "' already exist");
        }
        Wallet w(path.dup(), secret.dup());
        auto key = crypto::ECKey::generate();
        w.mData.MasterKey = key.getPublicKey().toString();
        w.mDirty = true;
        return w;
    }

    const String& Wallet::get(const suil::String &name) const
    {
        if (name.empty()) {
            return defaultKey();
        }

        auto lowerName = name.dup();
        lowerName.tolower();
        for (const auto& key : mData.Keys) {
            if (key.Name == lowerName) {
                return key.Key;
            }
        }
        return EMPTY;
    }

    const String& Wallet::generate(const suil::String &name)
    {
        auto lowerName = name.dup();
        lowerName.tolower();

        for (const auto& key : mData.Keys) {
            if (key.Name == lowerName) {
                throw Exception::create("A key with name '", name, "' already exists in wallet");
            }
        }
        auto key = crypto::ECKey::generate();
        Ego.mData.Keys.emplace_back(std::move(lowerName), key.getPublicKey().toString());
        Ego.mDirty = true;
        return Ego.mData.Keys.back().Key;
    }


    const String& Wallet::defaultKey() const
    {
        return Ego.mData.MasterKey;
    }

    void Wallet::save()
    {
        if (mDirty) {
            mDirty = false;
            Heapboard hb(1024);
            hb << Ego.mData;
            auto raw = hb.raw();
            auto encrypted = utils::AES_Encrypt(Ego.mSecret, raw);
            if (encrypted.empty()) {
                throw Exception::create("Encrypting wallet file failed");
            }
            if (utils::fs::exists(Ego.mPath())) {
                utils::fs::remove(Ego.mPath());
            }
            utils::fs::append(Ego.mPath(), encrypted.data(), encrypted.size(), false);
        }
    }


}