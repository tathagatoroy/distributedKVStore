cmake_minimum_required(VERSION 3.10)
project(distributedKVStore)


message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")

#use c++ 20 + 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#define test directory 
set(TEST_RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build/testExecutables)

#includes
include_directories(includes)

# test executables 
add_executable(serialize ./test/testSerialize.cpp)
set_target_properties(serialize PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_RUNTIME_DIR})

#test target 
enable_testing()
add_test(NAME runSerializeTest COMMAND ./build/testExecutables/serialize)

