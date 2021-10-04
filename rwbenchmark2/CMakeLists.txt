cmake_minimum_required(VERSION 3.10)

project(rwbenchmark VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O3")

link_libraries(ibverbs rdmacm pthread pmem)

add_executable(wrbenchmark src/wrbenchmark.c src/common.c)
add_executable(wsbenchmark src/wsbenchmark.c src/common.c)
add_executable(wsbenchmark_send_lat src/wsbenchmark_send_lat.c src/common.c)
add_executable(wibenchmark src/wibenchmark.c src/common.c)
add_executable(wbenchmark src/wbenchmark.c src/common.c)
add_executable(rbenchmark src/rbenchmark.c src/common.c)

install(TARGETS wrbenchmark DESTINATION bin)
install(TARGETS wsbenchmark DESTINATION bin)
install(TARGETS wibenchmark DESTINATION bin)
install(TARGETS wbenchmark DESTINATION bin)
install(TARGETS rbenchmark DESTINATION bin)
