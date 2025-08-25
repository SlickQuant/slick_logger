# slick_logger

A high-performance, cross-platform **header-only** logging library for C++23 using a multi-producer, multi-consumer ring buffer.

## Features

- **High Performance**: Asynchronous logging using slick_queue ring buffer for minimal latency
- **Header-Only**: No linking required - just include and use
- **Cross-Platform**: Supports Windows, Linux, and macOS
- **Multi-Threaded**: Safe for concurrent logging from multiple threads
- **C++20**: Utilizes modern C++ features
- **Easy to Use**: Simple macros for logging at different levels

## Requirements

- C++23 compatible compiler
- CMake 3.20 or higher (for building examples/tests)
- Internet connection for downloading slick_queue header

## Installation

Just copy the `include/` directory to your project and you're ready to go!

## Usage

```cpp
#include <slick_logger/logger.hpp>
#include <slick_logger/macros.hpp>

int main() {
    // Initialize the logger
    slick_logger::Logger::instance().init("app.log", 1024); // queue size

    // Log messages - formatting happens in background thread for performance
    LOG_INFO("Application started");
    LOG_DEBUG("Debug value: {}", 42);
    LOG_WARN("Processed {} items", 150);
    LOG_ERROR("Error in {} at line {}", "function_name", 123);
    LOG_INFO("User {} balance: ${:.2f}", "Alice", 1234.56);

    // Shutdown (optional, called automatically on destruction)
    slick_logger::Logger::instance().shutdown();

    return 0;
}
```

## Log Levels

- TRACE
- DEBUG
- INFO
- WARN
- ERR (internally named ERR to avoid Windows macro conflicts)
- FATAL

## Architecture

The logger uses a multi-producer, multi-consumer ring buffer (slick_queue) to queue log entries from multiple threads. A dedicated background thread dequeues and writes entries to the log file, ensuring that logging calls have minimal impact on application performance.

### Deferred Formatting

For optimal performance, the logger defers string formatting to the background thread:

1. **Caller Thread**: Captures format string and arguments, stores them in a lambda
2. **Queue**: Stores the lambda in the ring buffer (fast, lock-free operation)
3. **Background Thread**: Executes lambda to format string and write to file

This approach moves potentially expensive formatting operations off the critical path, making logging calls extremely fast and suitable for high-frequency logging scenarios.

## Integration

slick_queue is downloaded automatically during the build process from https://github.com/SlickTech/slick_queue.

## Header-Only Benefits

Being a header-only library provides several advantages:

- **Zero Linking**: No need to link against library files
- **Easy Integration**: Just include the headers in your project
- **Template Optimization**: Compiler can better optimize template instantiations
- **No Binary Dependencies**: No need to distribute or manage .lib/.a files
- **Immediate Usage**: Start logging with just two `#include` statements

## Building Examples/Tests

If you want to build the provided examples and tests:

```bash
mkdir build
cd build
cmake ..
make
```
