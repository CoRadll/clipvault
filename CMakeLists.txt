cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
project(clipvault LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SQLite 单独成一个库，core 和测试都要链接它
add_library(third_party_sqlite3 STATIC external/sqlite3/sqlite3.c)
target_include_directories(third_party_sqlite3 PUBLIC external/sqlite3)

# 核心逻辑库
add_library(clipvault_core STATIC
	src/core/textNormalize.cpp
	src/core/clipStore.cpp)
target_include_directories(clipvault_core PUBLIC include)
target_link_libraries(clipvault_core PUBLIC third_party_sqlite3)
if (MSVC)
	target_compile_options(clipvault_core PRIVATE /Zc:__cplusplus /permissive- /W4 /utf-8)
endif()

# 主程序
add_executable(clipvault
	src/main.cpp
	src/platform/win32/mainWindow.cpp
	src/platform/win32/clipboardReader.cpp)
target_link_libraries(clipvault PRIVATE clipvault_core)
if (WIN32)
	target_compile_definitions(clipvault PRIVATE UNICODE _UNICODE)
endif()
if (MSVC)
	target_compile_options(clipvault PRIVATE /Zc:__cplusplus /permissive- /W4 /utf-8)
endif()

# 测试
add_executable(clipvault_tests
	external/catch2/catch_amalgamated.cpp
	tests/test_textNormalize.cpp
	tests/test_clipStore.cpp)
target_include_directories(clipvault_tests PRIVATE external/catch2)
target_link_libraries(clipvault_tests PRIVATE clipvault_core)
if (MSVC)
	target_compile_options(clipvault_tests PRIVATE /Zc:__cplusplus /permissive- /W4 /utf-8)
endif()