cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(my_stl)

include_directories("${CMAKE_SOURCE_DIR}/include/")
#include_directories("../include/systemc/")
#include_directories("../include/spdlog/")
include_directories("${CMAKE_SOURCE_DIR}/include/gtest/")
include_directories("${CMAKE_SOURCE_DIR}/include/gmock/")
include_directories("${CMAKE_SOURCE_DIR}/test/")
#include_directories("${CMAKE_SOURCE_DIR}/common/")
#include_directories("${CMAKE_SOURCE_DIR}/peq/")
#include_directories("${CMAKE_SOURCE_DIR}/islip/")
#include_directories("${CMAKE_SOURCE_DIR}/dpa/")
#include_directories("${CMAKE_SOURCE_DIR}/test/")
#include_directories("${CMAKE_SOURCE_DIR}/network/")
#include_directories("${CMAKE_SOURCE_DIR}/sch/")

#link_directories("../libs/systemc/")
#link_directories("../libs/spdlog/")
link_directories("${CMAKE_SOURCE_DIR}/libs/gtest/")
link_directories("${CMAKE_SOURCE_DIR}/libs/gmock/")

add_executable(${PROJECT_NAME} 
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/test/test.cpp
)

target_link_libraries(${PROJECT_NAME} debug gtest              optimized gtest)
#target_link_libraries(${PROJECT_NAME} debug pthread            optimized pthread)
#target_link_libraries(${PROJECT_NAME} debug libsystemc_debug.a optimized libsystemc.a)
#target_link_libraries(${PROJECT_NAME} debug libspdlog.a        optimized libspdlog.a)