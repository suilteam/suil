cmake_minimum_required(VERSION 3.8)
project(suil C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

include(SuilUtils)

set(SUIL_MAJOR_VERSION 0 CACHE STRING "Build major version")
set(SUIL_MINOR_VERSION 0 CACHE STRING "Build minor version")
set(SUIL_PATCH_VERSION 0 CACHE STRING "Build patch version")
set(SUIL_BUILD_NUMBER  0 CACHE STRING "Build number version")
set(SUIL_BUILD_TAG     devel CACHE STRING "Version tag string")
set(SUIL_SOFTWARE_NAME suil  CACHE STRING "The name of the server")
set(SUIL_DEBUG_TAG     1a30f8248a6b11e781846f6e6c696e65 CACHE STRING "The tag used by the debug application tag")
set(SUIL_BASE_DIR     ".suil" CACHE STRING "The base directory of suil application")
set(SUIL_ENABLE_TRACE ON CACHE STRING "Enable debug traces in debug builds")

set(SUIL_COPYRIGHT "Copyright (C) 2018, 2017-2018 Suilteam")
set(SUIL_VERSION    ${SUIL_MAJOR_VERSION}.${SUIL_MINOR_VERSION}.${SUIL_PATCH_VERSION})

option(SUIL_DEV_IDE            "Currently using an IDE" OFF)
option(SUIL_BUILD_UNIT_TEST    "Enable unit testing" ON)
option(SUIL_BUILD_EXAMPLES     "Enable building examples" ON)
option(SUIL_BUILD_APPS         "Enable building applications" ON)
option(SUIL_BUILD_DEV_PACKAGE  "Produce a development package" OFF)
option(SUIL_BUILD_SAWTOOTH     "Enable building sawtooth SDK library" ON)
option(SUIL_BUILD_DOCKER       "Include docker API build" OFF)
option(SUIL_BUILD_LXC          "Include LXC API build" OFF)

# route used specifically by the file server
set(SUIL_FILE_SERVER_ROUTE     1511d5908a7811e790ed6f6e6c696e65)

set(SUIL_DEFINES)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SUIL_BUILD_DEBUG ON)
    set(SUIL_DEFINES "${SUIL_DEFINES} -DSUIL_DEBUG")
    if (SUIL_ENABLE_TRACE)
        set(SUIL_DEFINES "${SUIL_DEFINES} -DSUIL_TRACE_ENABLED")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings -Wno-attributes -fpermissive -Wno-expansion-to-defined -Wno-implicit-fallthrough -Wno-unused-result -Wno-format-security")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-expansion-to-defined -Wno-implicit-fallthrough -Wno-unused-result")
set(CMAKE_CXX_FLAGS_DEBUG   "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-z now")
set(CMAKE_SHARED_LINKER_FLAGS  ${CMAKE_SHARED_LINKER_FLAGS} "-z now")

include(CheckSymbolExists)
include(CheckFunctionExists)

SuilCheckFunctions()
SuilCheckLibrary(uuid INCLUDE uuid/uuid.h)
SuilCheckLibrary(sqlite3 LIBRARY sqlite3 libsqlite3)

# Configuration file
configure_file(
        ${CMAKE_SOURCE_DIR}/base.cmake.in
        ${CMAKE_BINARY_DIR}/.generated/suil/base.inc)
include_directories(${CMAKE_BINARY_DIR}/.generated)

find_package(OpenSSL REQUIRED)
if (OpenSSL_FOUND)
    set(SUIL_DEFINES "${SUIL_DEFINES} -DSSL_SUPPORTED")
endif()

find_package(PostgreSQL ${SUIL_PGSQL_VERSION} REQUIRED)

pkg_check_modules(ZeroMQ QUIET zmq)
if (NOT ZeroMQ_FOUND)
    pkg_check_modules(ZeroMQ QUIET libzmq)
endif()

if (NOT SUIL_DEV_IDE AND NOT ZeroMQ_FOUND)
   message(FATAL_ERROR "ZMQ development libraries required")
endif()

set(SUIL_LIBRARIES
        ssl crypto uuid sqlite3 pq zmq)

set(SUIL_STATIC_LIBRARIES
        ssl crypto uuid sqlite3 pq zmq lua)

set(SUIL_ARCHIVE_LIBS
        ${CMAKE_BINARY_DIR}/libmill_s.a
        ${CMAKE_BINARY_DIR}/libsnappy.a
        ${CMAKE_BINARY_DIR}/libsuil_s.a)

message(STATUS "Suil dependecies ${SUIL_LIBRARIES}")
message(STATUS "Suil static dependecies ${SUIL_STATIC_LIBRARIES}")

add_definitions(${SUIL_DEFINES})

# add dependencies projects
add_subdirectory(deps/libmill)
add_subdirectory(deps/iod)
add_subdirectory(deps/snappy)
add_subdirectory(deps/lua)

include_directories(deps)

set(SUIL_PACKAGE_INCLUDES  ${PostgreSQL_INCLUDE_DIRS})
set(SUIL_PACKAGE_DEFINES   ${SUIL_DEFINES})

if (SUIL_BUILD_DEV_PACKAGE)
    configure_file(buildapp.sh ${CMAKE_BINARY_DIR}/buildapp COPYONLY)

    install(DIRECTORY deps/libmill
            DESTINATION include
            FILES_MATCHING PATTERN "*.h")

    # library packaging
    set(SUIL_PACKAGE_STATIC_LIBRARIES ${SUIL_STATIC_LIBRARIES} suil)
    set(SUIL_PACKAGE_LIBRARIES ${SUIL_LIBRARIES} suil)

    # Create version configuration file
    configure_file(SuilVersion.cmake.in
            "${CMAKE_SOURCE_DIR}/cmake/SuilConfigVersion.cmake" @ONLY)

    install(DIRECTORY deps/iod
            DESTINATION include
            FILES_MATCHING PATTERN "*.hh")
    install(DIRECTORY deps/iod
            DESTINATION include
            FILES_MATCHING PATTERN "*.hpp")
    install(DIRECTORY deps/catch
            DESTINATION include
            FILES_MATCHING PATTERN "*.hh")
    install(DIRECTORY deps/catch
            DESTINATION include
            FILES_MATCHING PATTERN "*.hpp")
    install(DIRECTORY suil/
            DESTINATION include/suil
            FILES_MATCHING PATTERN "*.hh")
    install(DIRECTORY suil/
            DESTINATION include/suil
            FILES_MATCHING PATTERN "*.hpp")
    install(DIRECTORY suil/
            DESTINATION include/suil
            FILES_MATCHING PATTERN "*.h")
    install(DIRECTORY cmake/
            DESTINATION share/cmake/Modules
            FILES_MATCHING PATTERN "*.cmake")
    install(FILES ${CMAKE_BINARY_DIR}/.generated/suil/base.inc
            DESTINATION include/suil)
else()
    # library packaging
    set(SUIL_PACKAGE_STATIC_LIBRARIES ${SUIL_STATIC_LIBRARIES} suil)
    set(SUIL_PACKAGE_LIBRARIES ${SUIL_LIBRARIES} suil)
    # Create version configuration file
    configure_file(SuilVersion.cmake.in
            "${CMAKE_SOURCE_DIR}/cmake/SuilConfigVersion.cmake" @ONLY)
endif()

include_directories(${PostgreSQL_INCLUDE_DIRS})
include_directories(SYSTEM ${PROJECT_SOURCE_DIR})

# add the main project
add_subdirectory(suil)

include_directories(tools)
add_subdirectory(tools)

if (SUIL_BUILD_DEV_PACKAGE)
    message(STATUS "Development package enabled")
    set(SUIL_PACKAGE_POSTFIX dev)
else()
    message(STATUS "Production package enabled")
    set(SUIL_PACKAGE_POSTFIX prod)
endif()

if (SUIL_DEV_IDE)
    if (SUIL_BUILD_EXAMPLES)
        add_subdirectory(suil-examples)
    endif()
    if (SUIL_BUILD_APPS)
        add_subdirectory(apps/suilauth)
        add_subdirectory(apps/semausu)
        add_subdirectory(apps/suiltrader)
    endif()
endif()

set(CPACK_PACKAGE_NAME   "suil")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CPACK_PACKAGE_NAME "${CPACK_PACKAGE_NAME}dbg")
endif()
set(CPACK_PACKAGE_VENDOR "Suil Team")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Carter Mbotho")
set(CPACK_PACKAGE_VERSION_MAJOR ${SUIL_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${SUIL_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${SUIL_PATCH_VERSION})
set(CPACK_PACKAGE_VERSION ${SUIL_MAJOR_VERSION}.${SUIL_MINOR_VERSION}.${SUIL_PATCH_VERSION}-${SUIL_BUILD_TAG})
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${SUIL_PACKAGE_POSTFIX}_${CPACK_PACKAGE_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_GENERATOR "TGZ")

include(CPack)
