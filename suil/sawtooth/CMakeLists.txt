cmake_minimum_required(VERSION 3.5)
project(libsawtooth C CXX)

set(Protobuf_DEBUG ON)
set(Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf QUIET)

if(Protobuf_VERSION)
  message(STATUS "Using Protocol Buffers ${Protobuf_VERSION}")
endif()

set(SAWTOOTH_PROTOS
    protos/authorization.proto
    protos/batch.proto
    protos/block.proto
    protos/client_batch.proto
    protos/client_batch_submit.proto
    protos/client_block.proto
    protos/client_event.proto
    protos/client_list_control.proto
    protos/client_peers.proto
    protos/client_receipt.proto
    protos/client_state.proto
    protos/client_status.proto
    protos/client_transaction.proto
    protos/consensus.proto
    protos/events.proto
    protos/genesis.proto
    protos/identity.proto
    protos/merkle.proto
    protos/network.proto
    protos/processor.proto
    protos/setting.proto
    protos/state_context.proto
    protos/transaction.proto
    protos/transaction_receipt.proto
    protos/validator.proto)

protobuf_generate_cpp(SAWTOOTH_PROTO_SRCS SAWTOOTH_PROTO_HDRS ${SAWTOOTH_PROTOS})

set(SAWTOOTH_SOURCES
    sdk.cpp
        src/dispatcher.cpp
        src/gstate.cpp
        src/tp.cpp
        src/transaction.cpp
        src/stream.cpp)

include_directories(PROTOBUF_INCLUDE_DIR)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(sawtooth SHARED ${SAWTOOTH_PROTO_SRCS} ${SAWTOOTH_SOURCES})
target_link_libraries(sawtooth protobuf suil)

set(sawtooth_INSTALL_TARGETS sawtooth)

set(SAWTOOTH_PROTOS_ALL ${CMAKE_CURRENT_BINARY_DIR}/protos.h)
file(WRITE ${SAWTOOTH_PROTOS_ALL} "#ifndef __SUIL_SAWTOOTH_PROTOS_AMALGAMATE_H_\n")
file(APPEND ${SAWTOOTH_PROTOS_ALL} "#define __SUIL_SAWTOOTH_PROTOS_AMALGAMATE_H_\n")
foreach(HEADER ${SAWTOOTH_PROTO_HDRS})
    get_filename_component(HEADER_NAME ${HEADER} NAME)
    file(APPEND ${SAWTOOTH_PROTOS_ALL} "#include <suil/sawtooth/protos/${HEADER_NAME}>\n")
endforeach()
file(APPEND ${SAWTOOTH_PROTOS_ALL} "#endif // __SUIL_SAWTOOTH_PROTOS_AMALGAMATE_H_\n")

install(TARGETS ${sawtooth_INSTALL_TARGETS}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)


if (SUIL_BUILD_DEV_PACKAGE)
    install(FILES ${SAWTOOTH_PROTO_HDRS}
        DESTINATION include/suil/sawtooth/protos)
    install(FILES ${SAWTOOTH_PROTOS_ALL}
        DESTINATION include/suil/sawtooth)
endif()