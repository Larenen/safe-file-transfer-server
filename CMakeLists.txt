cmake_minimum_required(VERSION 3.14)
project(sft C)

set(CMAKE_C_STANDARD 99)

SET(CMAKE_C_FLAGS -pthread)

add_executable(sft main.c)
