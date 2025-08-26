cmake_minimum_required(VERSION 3.20)

project(slick_logger VERSION 0.1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(src/version.hpp.in ${CMAKE_SOURCE_DIR}/include/slick_logger/version.hpp)

# Download slick_queue header
set(slick_queue_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/slick_queue")
file(DOWNLOAD
    https://raw.githubusercontent.com/SlickTech/slick_queue/main/include/slick_queue.h
    ${slick_queue_SOURCE_DIR}/include/slick_queue.h
    STATUS download_status
)
list(GET download_status 0 download_result)
if(NOT download_result EQUAL 0)
    message(FATAL_ERROR "Failed to download slick_queue.h")
endif()

# Include directories
include_directories(${slick_queue_SOURCE_DIR}/include)
include_directories(include)

# Add header-only library
add_library(slick_logger INTERFACE)
target_include_directories(slick_logger INTERFACE include ${slick_queue_SOURCE_DIR}/include)

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
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
