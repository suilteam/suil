//
// Created by dc on 2020-03-02.
//

#include <cstring>
#include <exception>
#include <sstream>
#include <chrono>

#include <lua/lua.hpp>
#include <libmill/libmill.h>
#include <suil/buffer.h>
#include <suil/logging.h>
#include <suil/utils.h>

#include "scripts.h"

#include "_decls.h"

namespace {
    void exportFunction(lua_State *L, lua_CFunction func, const char *name)
    {
        lua_pushstring(L, name);
        lua_pushcfunction(L, func);
        lua_settable(L, -3);
    }
}

namespace suil::swept {

    static const embedded_script sScripts[] = {
#include "_defs.h"
    };

    lua_State *EmbeddedScripts::L{nullptr};

#define CALL_SYNTAX(func) \
if (!lua_istable(L, 1)) {   \
    return luaL_error(L, "use Swept:%s instead of Swep.%s", func); \
}

    int EmbeddedScripts::sweptLoad(lua_State *L)
    {
        CALL_SYNTAX("load")

        if (!lua_isstring(L, 2)) {
            if (lua_isnil(L, 2))
                lua_pushstring(L, "");
            else
                return luaL_error(L, "invalid parameter type, only 'string' accepted");
        }
        else {
            const char *name = lua_tostring(L, 2);
            return EmbeddedScripts::load(name);
        }
    }

    int EmbeddedScripts::sweptNow(lua_State *L)
    {
        CALL_SYNTAX("now")

        lua_pushnumber(L, mnow());
        return 1;
    }

    int EmbeddedScripts::sweptSleep(lua_State *L)
    {
        CALL_SYNTAX("sleep")

        if (!lua_isnumber(L, 2)) {
            return luaL_error(L, "Swept:sleep - invalid argument type, only number accepted");
        }

        auto ms = static_cast<int64_t >(luaL_checknumber(L, 2));
        msleep(now()+ms);
        return 0;
    }

    int EmbeddedScripts::sweptEnv(lua_State *L)
    {
        CALL_SYNTAX("env")
        if (!lua_isstring(L, 2)) {
            return luaL_error(L, "Swept:env takes name name of the environment variable");
        }
        lua_pushstring(L, utils::env(luaL_checkstring(L, 2), ""));
        return 1;
    }

    int EmbeddedScripts::sweptExit(lua_State *L)
    {
        CALL_SYNTAX("env")
        if (!lua_isnumber(L, 2)) {
            return luaL_error(L, "Swept:env takes name name of the environment variable");
        }
        auto num = static_cast<int>(luaL_checknumber(L, 2));
        lua_close(L);
        exit(num);
    }

    int EmbeddedScripts::sweptGc(lua_State *L)
    {
        CALL_SYNTAX("env")
        lua_gc(L, LUA_GCCOLLECT, 0);
        return 0;
    }

    int EmbeddedScripts::panicHandler(lua_State *L)
    {
        auto msg = luaL_checkstring(L, -1);
        if (msg) {
            fprintf(stderr, "error: %s\n", msg);
        }
        else {
            fprintf(stderr, "unknown error in swept\n");
        }
        exit(EXIT_FAILURE);
        return 0;
    }

    int EmbeddedScripts::error(const String &msg)
    {
        return luaL_error(state(), msg());
    }

    size_t EmbeddedScripts::count() {
        static size_t numScripts{sizeof(sScripts)/sizeof(embedded_script)};
        return numScripts;
    }

    lua_State* EmbeddedScripts::state() {
        if (L == nullptr) {
            // create lua runtime
            L = luaL_newstate();
            if (L == nullptr) {
                throw std::runtime_error("failed to create lua runtime");
            }
            luaL_openlibs(L);
        }

        return L;
    }

    int EmbeddedScripts::load(const char *name)
    {
        const embedded_script *found{nullptr};

        for (auto i = 0; i < count()-1; i++) {
            if (strcmp(name, sScripts[i].name) == 0) {
                found = &sScripts[i];
                break;
            }
        }

        if (found == nullptr) {
            OBuffer ob;
            ob << "internal module '" << name << "' does not exist";
            return EmbeddedScripts::error(ob);
        }
        else {
            return loadScript(*found);
        }
    }

    int EmbeddedScripts::loadScript(const embedded_script &es)
    {
        auto status = luaL_loadbuffer(state(), es.buffer, es.size, es.name);
        if (status) {
            return luaL_error(state(), "%s", lua_tostring(state(), -1));
        }
        status = lua_pcall(state(), 0, 1, 0);
        if (status) {
            return luaL_error(state(), "%s", lua_tostring(L, -1));
        }
        return 1;
    }

    void EmbeddedScripts::exportApis()
    {
        exportFunction(state(), EmbeddedScripts::sweptLoad, "load");
        exportFunction(state(), EmbeddedScripts::sweptNow,  "now");
        exportFunction(state(), EmbeddedScripts::sweptSleep, "sleep");
        exportFunction(state(), EmbeddedScripts::sweptEnv, "env");
        exportFunction(state(), EmbeddedScripts::sweptGc, "gc");
        exportFunction(state(), EmbeddedScripts::sweptExit, "exit");
    }

    static void createargtable (lua_State *L, char **argv, int argc, int script) {
        int i, narg;
        if (script == argc) script = 0;  /* no script name? */
        narg = argc - (script + 1);  /* number of positive indices */
        lua_createtable(L, narg, script + 1);
        for (i = 0; i < argc; i++) {
            lua_pushstring(L, argv[i]);
            lua_rawseti(L, -2, i - script);
        }
        lua_setglobal(L, "arg");
    }

    void EmbeddedScripts::main(int argc, char **argv)
    {
        // export some runtime native API's
        lua_atpanic(state(), EmbeddedScripts::panicHandler);
        luaL_newmetatable(state(), "Swept_mt");
        lua_pushstring(state(), "__index");
        lua_newtable(state());
        EmbeddedScripts::exportApis();
        lua_settable(state(), -3);
        lua_newtable(state());
        luaL_getmetatable(state(), "Swept_mt");
        lua_setmetatable(L, -2);
        lua_setglobal(state(), "Swept");

        // build commandline arguments table
        lua_createtable(state(), argc, 1);
        for (int i = 0; i < argc; i++) {
            lua_pushstring(L, argv[i]);
            lua_rawseti(state(), -2, i);
        }
        lua_setglobal(state(), "arg");
    }
}