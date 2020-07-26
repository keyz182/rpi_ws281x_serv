cmake_minimum_required(VERSION 3.18)
project(rpc VERSION 1.0)

set(CMAKE_BUILD_TYPE Debug)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(ZMQ REQUIRED IMPORTED_TARGET libzmq)

set (scons_cmd "scons")
message(STATUS "Hello")
execute_process(COMMAND ${scons_cmd}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/rpi_ws281x)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ../ws281x.proto)

add_executable(server server.cc ${PROTO_SRCS} ${PROTO_HDRS})

include_directories(
    ${ZMQ_INCLUDE_DIRS} 
)

target_link_libraries(server ${Protobuf_LIBRARIES})
target_link_libraries(server ${ZMQ_LIBRARIES})
target_link_libraries(server ${PROJECT_SOURCE_DIR}/rpi_ws281x/libws2811.a)
target_link_libraries(server Threads::Threads)
target_include_directories(server PUBLIC ${ZMQ_INCLUDE_DIRS})

target_compile_options(server PUBLIC ${ZMQ_CFLAGS_OTHER})
