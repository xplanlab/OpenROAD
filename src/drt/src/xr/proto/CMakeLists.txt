find_package(Protobuf REQUIRED)

set(PROTO_FILES
        xroute/net_ordering.proto
)

add_library(xroute_proto ${PROTO_FILES})

target_link_libraries(xroute_proto
    PUBLIC
        protobuf::libprotobuf
)

target_include_directories(xroute_proto
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

protobuf_generate(TARGET xroute_proto LANGUAGE cpp)
