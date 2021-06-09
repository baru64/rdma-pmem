cmake_minimum_required(VERSION 3.10)

project(rdma-benchmark VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

link_libraries(ibverbs rdmacm)

add_executable(client src/client.cpp src/common.cpp)
add_executable(server src/server.cpp) 

install(TARGETS client server DESTINATION bin)
