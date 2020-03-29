//
// Created by dc on 2020-03-02.
//

#ifndef SUIL_SCRIPTS_H
#define SUIL_SCRIPTS_H

#include <cstddef>
#include <map>

struct lua_State;

namespace suil::swept {

    struct embedded_script {
        const char *name;
        size_t      size;
        const char *buffer;
    };

    struct EmbeddedScripts {
        static int load(const char *name);
        static void main(int argc, char **argv);

    private:
        static int sweptNow(lua_State *L);
        static int sweptLoad(lua_State* L);
        static int sweptSleep(lua_State* L);
        static int sweptEnv(lua_State* L);
        static int sweptBase64Encode(lua_State* L);
        static int sweptBase64Decode(lua_State* L);
        static int sweptGc(lua_State* L);
        static int sweptExit(lua_State* L);
        static int panicHandler(lua_State* L);

    private:
        static int error(const String& msg);
        static size_t count();
        static void exportApis();
        static int loadScript(const embedded_script& es);
        static lua_State *state();
        static lua_State *L;
    };

}

#endif //SUIL_SCRIPTS_H
