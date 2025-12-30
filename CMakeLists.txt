cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(my_stl)

add_definitions(-DPROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

include_directories("${CMAKE_SOURCE_DIR}/include/")
include_directories("${CMAKE_SOURCE_DIR}/include/systemc/")
include_directories("${CMAKE_SOURCE_DIR}/include/spdlog/")
include_directories("${CMAKE_SOURCE_DIR}/include/gtest/")
include_directories("${CMAKE_SOURCE_DIR}/include/gmock/")
include_directories("${CMAKE_SOURCE_DIR}/include/tinyxml2/")
include_directories("${CMAKE_SOURCE_DIR}/test/")
include_directories("${CMAKE_SOURCE_DIR}/modules/top/")
include_directories("${CMAKE_SOURCE_DIR}/modules/node/")
include_directories("${CMAKE_SOURCE_DIR}/modules/gen/")
include_directories("${CMAKE_SOURCE_DIR}/modules/memory/")
include_directories("${CMAKE_SOURCE_DIR}/modules/dmac/")
include_directories("${CMAKE_SOURCE_DIR}/modules/peq/")
#include_directories("${CMAKE_SOURCE_DIR}/common/")
#include_directories("${CMAKE_SOURCE_DIR}/peq/")
include_directories("${CMAKE_SOURCE_DIR}/modules/islip/")
include_directories("${CMAKE_SOURCE_DIR}/modules/dpa/")
#include_directories("${CMAKE_SOURCE_DIR}/test/")
#include_directories("${CMAKE_SOURCE_DIR}/network/")
#include_directories("${CMAKE_SOURCE_DIR}/sch/")

link_directories("${CMAKE_SOURCE_DIR}/libs/systemc/")
link_directories("${CMAKE_SOURCE_DIR}/libs/spdlog/")
link_directories("${CMAKE_SOURCE_DIR}/libs/gtest/")
link_directories("${CMAKE_SOURCE_DIR}/libs/gmock/")
link_directories("${CMAKE_SOURCE_DIR}/libs/tinyxml2/")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
#set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

add_executable(${PROJECT_NAME} 
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/test/test.cpp
    ${CMAKE_SOURCE_DIR}/test/peq_test.cpp
    ${CMAKE_SOURCE_DIR}/test/pcie_scan.cpp
    ${CMAKE_SOURCE_DIR}/test/pcie_vc.cpp
    ${CMAKE_SOURCE_DIR}/test/pcie_arb.cpp
    ${CMAKE_SOURCE_DIR}/modules/node/node.cpp
    ${CMAKE_SOURCE_DIR}/modules/memory/memory.cpp
    ${CMAKE_SOURCE_DIR}/modules/top/top.cpp
    ${CMAKE_SOURCE_DIR}/modules/islip/islip.cpp
    ${CMAKE_SOURCE_DIR}/modules/islip/booksim_islip.cpp
    ${CMAKE_SOURCE_DIR}/modules/islip/fifo.cpp
    ${CMAKE_SOURCE_DIR}/modules/islip/pim.cpp
    ${CMAKE_SOURCE_DIR}/modules/islip/rrm.cpp
    ${CMAKE_SOURCE_DIR}/modules/islip/islip_basic.cpp
    ${CMAKE_SOURCE_DIR}/modules/islip/islip_sp.cpp
    ${CMAKE_SOURCE_DIR}/modules/islip/islip_threshold.cpp
    ${CMAKE_SOURCE_DIR}/modules/islip/islip_wrr.cpp
    ${CMAKE_SOURCE_DIR}/modules/dpa/dpa.cpp
)

target_link_libraries(${PROJECT_NAME} debug gtest              optimized gtest)
target_link_libraries(${PROJECT_NAME} debug pthread            optimized pthread)
target_link_libraries(${PROJECT_NAME} debug libsystemc_debug.a optimized libsystemc.a)
target_link_libraries(${PROJECT_NAME} debug libspdlog.a        optimized libspdlog.a)
target_link_libraries(${PROJECT_NAME} debug libtinyxml2.a      optimized libtinyxml2.a)