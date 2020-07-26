cmake_minimum_required(VERSION 3.13)
project(rpc VERSION 1.0)

set(CMAKE_BUILD_TYPE Debug)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(ZMQ REQUIRED IMPORTED_TARGET libzmq)

add_subdirectory(server)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ws281x.proto)

add_executable(test server/rpi_ws281x/main.c ${PROTO_SRCS} ${PROTO_HDRS})
add_library(libws2811_remote client.cc ${PROTO_SRCS} ${PROTO_HDRS})

include_directories(
    ${ZMQ_INCLUDE_DIRS} 
)

target_link_libraries(libws2811_remote ${Protobuf_LIBRARIES})
target_link_libraries(libws2811_remote ${ZMQ_LIBRARIES})
target_include_directories(libws2811_remote PUBLIC ${ZMQ_INCLUDE_DIRS})

target_link_libraries(test ${Protobuf_LIBRARIES})
target_link_libraries(test ${ZMQ_LIBRARIES})
target_link_libraries(test libws2811_remote)
target_include_directories(test PUBLIC ${ZMQ_INCLUDE_DIRS})

target_compile_options(test PUBLIC ${ZMQ_CFLAGS_OTHER})
target_compile_options(libws2811_remote PUBLIC ${ZMQ_CFLAGS_OTHER})
