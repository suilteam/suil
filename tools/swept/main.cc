#include <suil/init.h>
#include <suil/cmdl.h>

#include "scripts.h"

using namespace suil;

int main(int argc, char *argv[])
{
    suil::init(opt(printinfo, false), opt(skiproc, true));
    suil::log::setup(opt(verbose, 4));

    swept::EmbeddedScripts::main(argc, argv);
    swept::EmbeddedScripts::load("swept_main");
    return EXIT_SUCCESS;
}