cmake_minimum_required(VERSION 3.20)
project(spaceglide C)

set(CMAKE_C_STANDARD 11)

find_package(raylib REQUIRED)

add_executable(spaceglide main.c)

target_compile_options(spaceglide PRIVATE
        -fsanitize=address
        -fno-omit-frame-pointer
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
        -Og
        -g
)
target_link_options(spaceglide PRIVATE
    -fsanitize=address)

target_include_directories(spaceglide PRIVATE include)

target_link_libraries(spaceglide PRIVATE raylib m)
