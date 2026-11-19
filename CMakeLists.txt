cmake_minimum_required(VERSION 3.28)
project(clipvault LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(clipvault src/main.cpp src/util.cpp)

if (MSVC)
	target_compile_options(clipvault PRIVATE /Zc:__cplusplus /permissive- /W4)
endif()