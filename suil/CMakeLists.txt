cmake_minimum_required(VERSION 3.5)
project(libsuil C CXX)

set(LIB_SUIL_BASE_SOURCES
        base.cpp
        base64.cpp
        buffer.cpp
        channel.cpp
        cmdl.cpp
        compress.cpp
        config.cpp
        console.cpp
        crypto.cpp
        email.cpp
        file.cpp
        init.cpp
        json.cpp
        mustache.cpp
        logging.cpp
        net.cpp
        process.cpp
        redis.cpp
        secp256k1.cpp
        sock.cpp
        utils.cpp
        wire.cpp
        varint.cpp
        worker.cpp
        zstring.cpp)

set(LIB_SUIL_HTTP_SOURCES
        http/auth.cpp
        http/client.cpp
        http/cors.cpp
        http/fserver.cpp
        http/middlewares.cpp
        http/parser.cpp
        http/qstring.cpp
        http/request.cpp
        http/response.cpp
        http/routing.cpp
        http/wsock.cpp
        http/validators.cpp)

set(LIB_SUIL_SQL_SOURCES
        sql/pgsql.cpp)

set(LIB_SUIL_RPC_SOURCES
        rpc/utils.cpp
        rpc/jsonrpc.cpp
        rpc/suilrpc.cpp)

set(LIB_SUIL_ZMQ_SOURCES
        zmq/zmq.cpp)

set(LIB_SUIL_TMSP_SOURCES
        tmsp/client.cpp
        tmsp/server.cpp
        tmsp/types.pb.cc)

# only include docker sources when enables
set(LIB_SUIL_DOCKER_SOURCES)

set(LIB_SUIL_SOURCES
        ${LIB_SUIL_BASE_SOURCES}
        ${LIB_SUIL_HTTP_SOURCES}
        ${LIB_SUIL_RPC_SOURCES}
        ${LIB_SUIL_SQL_SOURCES}
        ${LIB_SUIL_ZMQ_SOURCES})

set_property(GLOBAL PROPERTY prop_SUIL_SOURCES
        ${LIB_SUIL_SOURCES})

# add a shared and a static library
add_library(suil SHARED
        ${LIB_SUIL_SOURCES})

target_link_libraries(suil ${SUIL_LIBRARIES}  secp256k1 mill_s snappy lua)

if (SUIL_BUILD_SAWTOOTH)
    add_subdirectory(sawtooth)
endif()

# generate all symbols used in suil project
suil_iod_symbols(suil
        BINARY  ${CMAKE_BINARY_DIR}/deps/iod/iodsyms
        SYMBOLS ${CMAKE_SOURCE_DIR}/suil/suil.sym
        OUTPUT  ${CMAKE_SOURCE_DIR}/suil/symbols.h
        DEPENDS iodsyms)

# change output directory of suil
set_target_properties(suil
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if (SUIL_BUILD_UNIT_TEST)
    get_property(_SUIL_SOURCES GLOBAL PROPERTY prop_SUIL_SOURCES)
    file(GLOB_RECURSE SUIL_TEST_SOURCES ../tests/*.cpp ../test/*.c)
    add_executable(sut ${_SUIL_SOURCES} ${SUIL_TEST_SOURCES})
    add_dependencies(sut suil-gensyms)
    target_link_libraries(sut ${SUIL_LIBRARIES} secp256k1 mill_s snappy lua)
    target_compile_definitions(sut PUBLIC -Dunit_test -Dsuil_ut=:public)
endif()

set(suil_INSTALL_TARGETS suil)

install(TARGETS ${suil_INSTALL_TARGETS}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)