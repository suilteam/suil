//
// Created by dc on 2020-01-15.
//

#include <suil/init.h>
#include <suil/cmdl.h>
#include "../client.h"

using namespace suil;

static void cmdCreate(cmdl::Parser& parser);
static void cmdGenerate(cmdl::Parser& parser);
static void cmdGet(cmdl::Parser& parser);

int main(int argc, char *argv[])
{
    suil::init(opt(printinfo, false));
    sawsdk::Client::Logger logger;
    log::setup(opt(verbose, 0));

    cmdl::Parser parser("stw", "1.0", "A command line application used create/edit a wallet file");
    try {
        // add commands and run
        cmdCreate(parser);
        cmdGenerate(parser);
        cmdGet(parser);

        parser.parse(argc, argv);
        parser.handle();

        return EXIT_SUCCESS;
    }
    catch(...) {
        // unhandled Exception
        serror("%s", Exception::fromCurrent().what());
        return EXIT_FAILURE;
    }
}

void cmdCreate(cmdl::Parser& parser)
{
    cmdl::Cmd create{"create", "Create a new wallet file initialized with a master key"};
    create << cmdl::Arg{"secret", "The secret used to lock/unlock the wallet file",
                        cmdl::NOSF, false, true};
    create << cmdl::Arg{"path", "Path to save the created wallet file to (default: .sawtooth.key)",
                        cmdl::NOSF, false};

    create([](cmdl::Cmd& cmd) {
        auto wallet = sawsdk::Client::Wallet::create(cmd.getvalue("path", ".sawtooth.key"), cmd["secret"]);
        wallet.save();
    });
    parser.add(std::move(create));
}

void cmdGenerate(cmdl::Parser& parser)
{
    cmdl::Cmd get{"generate", "Generate and add new key to the given wallet file"};
    get << cmdl::Arg{"secret", "The secret used to lock/unlock the wallet file",
                        cmdl::NOSF, false, true};
    get << cmdl::Arg{"path", "Path to the wallet file (default: .sawtooth.key)",
                        cmdl::NOSF, false};
    get << cmdl::Arg{"name", "The name of the new key to generate",
                     cmdl::NOSF, false, true};

    get([](cmdl::Cmd& cmd) {
        auto wallet = sawsdk::Client::Wallet::open(cmd.getvalue("path", ".sawtooth.key"), cmd["secret"]);
        wallet.generate(cmd["name"]);
        wallet.save();
    });
    parser.add(std::move(get));
}

void cmdGet(cmdl::Parser& parser)
{
    cmdl::Cmd get{"get", "Get a key from the given wallet file"};
    get << cmdl::Arg{"secret", "The secret used to unlock/lock the wallet file",
                     cmdl::NOSF, false, true};
    get << cmdl::Arg{"path", "Path to the wallet file (default: .sawtooth.key)",
                     cmdl::NOSF, false};
    get << cmdl::Arg{"name", "The name of the new key to retrieve (if not given default key will be returned)",
                     cmdl::NOSF, false};

    get([](cmdl::Cmd& cmd) {
        auto wallet = sawsdk::Client::Wallet::open(cmd.getvalue("path", ".sawtooth.key"), cmd["secret"]);
        const auto& key = wallet.get(cmd.getvalue("name", ""));
        if (key.empty()) {
            serror("Key '%s' does not exist", key());
        }
        else {
            sinfo("%s", key());
        }
    });
    parser.add(std::move(get));
}
