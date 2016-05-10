cmake_minimum_required(VERSION 3.4)
project(cpp-fuse CXX)
enable_testing()

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/libfuse/include)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fsanitize=address -ggdb -O2 -Wall -pedantic -lutil -ldl -fPIC")

ADD_EXECUTABLE(cpp-fuse libfuse.cc FS.h)
TARGET_LINK_LIBRARIES(cpp-fuse ${CMAKE_CURRENT_SOURCE_DIR}/libfuse/lib/.libs/libfuse3.so)

