cmake_minimum_required(VERSION 3.20)

project(hello_world
    VERSION 1.0.0
    DESCRIPTION "A minimal C hello world project"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

add_executable(hello_world src/main.c)

target_compile_options(hello_world PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

add_library(stm src/stm/stm.c)
target_include_directories(stm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_options(stm PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
