cmake_minimum_required(VERSION 3.14)

project(Thread_pool VERSION 1.0 LANGUAGES CXX)

set_target_properties(Thread_pool ROPERTIES
CXX_STANDARD 17
CXX_STANDARD_REQUIRED YES)

set(SOURCE
main.cpp
thread_poolcpp)

add_executable(Thread_pool ${SOURCE})
