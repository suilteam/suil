//
// Created by dc on 20/06/17.
//
#include <suil/http/endpoint.h>
#include <suil/http/fserver.h>
#include <suil/sql/sqlite.h>
#include <suil/sql/pgsql.h>
#include <suil/app.hpp>
#include <suil/http/clientapi.h>
#include <suil/email.h>

using  namespace suil;
using  namespace suil::http;


int main(int argc, const char *argv[])
{
    suil::init();
    suil::log::setup(opt(verbose, 3));
    Endpoint<> ep("/api",
            opt(port, 1024),
            opt(name, "0.0.0.0"));
    FileServer fs(ep,
            opt(root, ""));
    fs.alias("/banner", "images/banner.mp4");

    // simple route
    ep("/hello/<string>")
    ("GET"_method)
    ([](std::string name) {
        // simple example returns the name
        return utils::catstr("Hello ", name);
    });

    ep.start();
}
