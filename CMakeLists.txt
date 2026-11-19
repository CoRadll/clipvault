cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
project(clipvault LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(clipvault src/main.cpp external/sqlite3/sqlite3.c)

target_include_directories(clipvault PRIVATE external/sqlite3 include)

if (MSVC)
	target_compile_options(clipvault PRIVATE /Zc:__cplusplus /permissive- /W4 /utf-8)
endif()

add_executable(clipvault_tests external/catch2/catch_amalgamated.cpp tests/test_textNormalize.cpp src/core/textNormalize.cpp)

target_include_directories(clipvault_tests PRIVATE external/catch2 include)

if (MSVC)
	target_compile_options(clipvault_tests PRIVATE /Zc:__cplusplus /permissive- /W4 /utf-8)
endif()