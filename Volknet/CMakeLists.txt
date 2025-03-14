
cmake_minimum_required (VERSION 2.8)
 
project (volknet)
set(CMAKE_CXX_STANDARD 20)
include_directories(../include)
find_package(OpenSSL REQUIRED)
find_package(NanoMsg REQUIRED)
add_executable(volknet main.cpp)
