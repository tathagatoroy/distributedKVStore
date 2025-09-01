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

# Find the threads library, needed for std::thread
find_package(Threads REQUIRED)

# test executables 
add_executable(serialize ./test/testSerialize.cpp)
add_executable(testNetwork ./test/testNetwork.cpp)
target_link_libraries(testNetwork PRIVATE ws2_32 Threads::Threads)

add_executable(client ./examples/client.cpp)
target_link_libraries(client PRIVATE ws2_32 Threads::Threads)

add_executable(server ./examples/server.cpp)
target_link_libraries(server PRIVATE ws2_32 Threads::Threads)


set_target_properties(serialize PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_RUNTIME_DIR})
set_target_properties(testNetwork PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_RUNTIME_DIR})

#test target 
enable_testing()
add_test(NAME runSerializeTest COMMAND ./build/testExecutables/serialize)
add_test(NAME runNetworkTest COMMAND ./build/testExecutables/testNetwork 0)

# --- Custom Run Commands ---
# These targets make it easy to run from the command line after building.

# Add a custom target to run the server
add_custom_target(run_server
    COMMAND server.exe
    DEPENDS server
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running server..."
)

# Add a custom target to run the client
add_custom_target(run_client
    COMMAND client.exe
    DEPENDS client
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running client..."
)
