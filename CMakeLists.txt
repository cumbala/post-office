cmake_minimum_required(VERSION 3.25)
project(ios_proj2 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unknown-pragmas -Wextra -Werror -pedantic -pthread")

add_executable(proj2 proj2.c)