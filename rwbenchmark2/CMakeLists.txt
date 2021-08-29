cmake_minimum_required(VERSION 3.10)

project(rwbenchmark VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

link_libraries(ibverbs rdmacm pthread pmem)

add_executable(rwbenchmark src/rwbenchmark.c src/common.c)
add_executable(wsbenchmark src/wsbenchmark.c src/common.c)
add_executable(wibenchmark src/wibenchmark.c src/common.c)

install(TARGETS rwbenchmark DESTINATION bin)
install(TARGETS wsbenchmark DESTINATION bin)
install(TARGETS wibenchmark DESTINATION bin)
