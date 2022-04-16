cmake_minimum_required(VERSION 3.16)
project(Email)

set(CMAKE_CXX_STANDARD 14)
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/build)

add_executable(Email main.cpp)

include_directories(include)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_package(Threads)
    target_link_libraries(Email ${CMAKE_THREAD_LIBS_INIT})
endif()