cmake_minimum_required(VERSION 3.20)

project(slick_logger VERSION 0.1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Disable tests for slick_queue
set(BUILD_SLICK_QUEUE_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    slick_queue
    GIT_REPOSITORY https://github.com/SlickTech/slick_queue.git
    GIT_TAG main
)
FetchContent_MakeAvailable(slick_queue)

configure_file(src/logger.hpp ${CMAKE_SOURCE_DIR}/include/slick_logger/logger.hpp)

# Add header-only library
add_library(slick_logger INTERFACE)
target_include_directories(slick_logger INTERFACE ${slick_queue_SOURCE_DIR}/include
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
    add_definitions(-D_WIN32_WINNT=0x0A00)
    set(CMAKE_SUPPRESS_REGENERATION true)   # supress zero_check
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address,undefined")
endif()

# Add tests (placeholder)
enable_testing()
add_subdirectory(tests)

# Add examples (placeholder)
add_subdirectory(examples)

# Add benchmarks (optional)
option(BUILD_SLICK_LOGGER_BENCHMARKS "Build performance benchmarks" OFF)
if(BUILD_SLICK_LOGGER_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
