cmake_minimum_required(VERSION 3.10)

project(rwbenchmark VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

link_libraries(ibverbs rdmacm pthread)

add_executable(rwbenchmark src/rwbenchmark.c src/common.c)

install(TARGETS rwbenchmark DESTINATION bin)
