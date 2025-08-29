# slick_logger

A high-performance, cross-platform **header-only** logging library for C++20 using a multi-producer, multi-consumer ring buffer with **multi-sink support** and **log rotation** capabilities.

## Features

- **High Performance**: Asynchronous logging using slick_queue ring buffer for minimal latency
- **Multi-Sink Architecture**: Log to multiple destinations simultaneously (console, files, custom sinks)
- **Log Rotation**: Size-based and time-based rotation with configurable retention
- **Colored Console Output**: ANSI color support with configurable error routing
- **Header-Only**: No linking required - just include and use
- **Cross-Platform**: Supports Windows, Linux, and macOS
- **Multi-Threaded**: Safe for concurrent logging from multiple threads
- **C++20**: Utilizes modern C++ features
- **Easy to Use**: Simple macros for logging at different levels

## Requirements

- C++20 compatible compiler
- CMake 3.20 or higher (for building examples/tests)
- Internet connection for downloading slick_queue header

## Installation

### Option 1: Direct Copy
For manual installation, you need both slick_logger and its dependency:

1. Copy the `include/slick_logger/` directory to your project
2. Download `slick_queue.h` from https://raw.githubusercontent.com/SlickTech/slick_queue/main/include/slick_queue/slick_queue.h
3. Place `slick_queue.h` in your include path or alongside the slick_logger headers

Your project structure should look like:
```
your_project/
├── include/
│   ├── slick_logger/
│   │   └── logger.hpp
│   └── slick_queue/
│       └── slick_queue.h
└── src/
    └── main.cpp
```

### Option 2: CMake Integration (Recommended)

CMake automatically handles the slick_queue dependency for you.

#### Using FetchContent (Recommended)
```cmake
cmake_minimum_required(VERSION 3.20)
project(your_project)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Disable examples, tests, and benchmarks for slick_logger
set(BUILD_SLICK_LOGGER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SLICK_LOGGER_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SLICK_LOGGER_BENCHMARKS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    slick_logger
    GIT_REPOSITORY https://github.com/SlickTech/slick_logger.git
    GIT_TAG main
)

FetchContent_MakeAvailable(slick_logger)

add_executable(your_app main.cpp)
target_link_libraries(your_app slick_logger)
```

#### Using find_package
If you've installed slick_logger system-wide:

```cmake
cmake_minimum_required(VERSION 3.20)
project(your_project)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(slick_logger REQUIRED)

add_executable(your_app main.cpp)
target_link_libraries(your_app slick_logger)
```

#### Manual Integration
```cmake
cmake_minimum_required(VERSION 3.20)
project(your_project)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add slick_logger include directory
include_directories(path/to/slick_logger/include, path/to/slick_queue/include)

add_executable(your_app main.cpp)
```

## Usage

### Basic Usage

```cpp
#include <slick_logger/logger.hpp>

int main() {
    // Initialize the logger (traditional way)
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

### Multi-Sink Usage

```cpp
#include <slick_logger/logger.hpp>

int main() {
    using namespace slick_logger;
    
    // Setup multiple sinks
    Logger::instance().clear_sinks();
    Logger::instance().add_console_sink(true, true);     // colors + stderr for errors
    Logger::instance().add_file_sink("app.log");         // basic file logging
    
    // Configure rotation
    RotationConfig rotation;
    rotation.max_file_size = 10 * 1024 * 1024;  // 10MB
    rotation.max_files = 5;                      // keep last 5 files
    Logger::instance().add_rotating_file_sink("debug.log", rotation);
    
    // Initialize with queue size
    Logger::instance().init(8192);
    
    // Logs appear in console (colored) AND both files!
    LOG_INFO("Multi-sink logging is active!");
    LOG_ERROR("Errors go to stderr and files");
    
    Logger::instance().shutdown();
    return 0;
}
```

### Advanced Configuration

```cpp
#include <slick_logger/logger.hpp>

int main() {
    using namespace slick_logger;
    
    // Create configuration
    LogConfig config;
    config.sinks.push_back(std::make_shared<ConsoleSink>(true, true));
    config.sinks.push_back(std::make_shared<FileSink>("application.log"));
    
    // Add rotating file sink for errors
    RotationConfig rotation;
    rotation.max_file_size = 5 * 1024 * 1024;  // 5MB
    rotation.max_files = 10;
    config.sinks.push_back(std::make_shared<RotatingFileSink>("errors.log", rotation));
    
    // Add daily log files
    config.sinks.push_back(std::make_shared<DailyFileSink>("daily.log"));
    
    config.min_level = LogLevel::INFO;
    config.queue_size = 16384;
    
    Logger::instance().init(config);
    
    // Logs go to: console + application.log + errors.log + daily_YYYY-MM-DD.log
    LOG_INFO("Advanced multi-sink setup complete!");
    
    Logger::instance().shutdown();
    return 0;
}
```

## Sink Types

### ConsoleSink
Outputs to stdout/stderr with optional ANSI color support:
- **Colors**: Configurable color coding by log level
- **Error Routing**: WARN/ERROR/FATAL can go to stderr
- **Cross-Platform**: Works on Windows, Linux, macOS

### FileSink  
Basic file logging:
- **Append Mode**: Writes to specified file
- **Thread-Safe**: Single writer thread handles all file operations
- **Auto-Creation**: Creates directories if they don't exist

### RotatingFileSink
Size-based log rotation:
- **Max File Size**: Configurable size limit (default: 10MB)
- **File Retention**: Keep last N files, auto-delete oldest
- **Naming**: `log.txt` → `log_1.txt` → `log_2.txt` etc.
- **Atomic Rotation**: Thread-safe file rotation

### DailyFileSink
Date-based log rotation:
- **Daily Files**: Creates new file each day
- **Date Format**: `filename_YYYY-MM-DD.log`
- **Automatic**: Switches files at midnight
- **Retention**: Configurable cleanup of old files

## Rotation Configuration

```cpp
slick_logger::RotationConfig config;
config.max_file_size = 50 * 1024 * 1024;  // 50MB
config.max_files = 10;                     // keep last 10 files
config.compress_old = false;               // future feature
config.rotation_hour = std::chrono::hours(0); // midnight for daily rotation
```

## Log Levels

- **TRACE**: Detailed debug information
- **DEBUG**: General debug information  
- **INFO**: Informational messages
- **WARN**: Warning messages
- **ERR**: Error messages (internally named ERR to avoid Windows macro conflicts)
- **FATAL**: Fatal error messages

## Architecture

The logger uses a **multi-producer, single-consumer** ring buffer (slick_queue) with a **multi-sink architecture**:

```
[Thread 1] ──┐
[Thread 2] ──┼──► [Lock-Free Queue] ──► [Writer Thread] ──┬──► ConsoleSink
[Thread N] ──┘                                            ├──► FileSink  
                                                          ├──► RotatingFileSink
                                                          └──► DailyFileSink
```

### Key Design Principles

1. **Single Writer Thread**: One dedicated thread handles all sink operations
2. **Lock-Free Logging**: Caller threads never block on I/O operations  
3. **Flexible Sinks**: Easy to add custom sink implementations
4. **Atomic Operations**: Thread-safe queue and sink management

### Deferred Formatting

For optimal performance, the logger defers string formatting to the background thread:

1. **Caller Thread**: Captures format string and arguments in a lambda (fast)
2. **Lock-Free Queue**: Stores the lambda in the ring buffer (minimal latency)
3. **Writer Thread**: Executes lambda to format string and writes to all sinks

This approach moves potentially expensive formatting and I/O operations off the critical path, making logging calls extremely fast and suitable for high-frequency logging scenarios.

### Multi-Sink Benefits

- **Simultaneous Output**: Log to console + multiple files at once
- **Different Retention**: Each sink can have its own rotation policy
- **Performance**: Single writer thread efficiently handles all sinks
- **Flexibility**: Mix and match sink types as needed

## Integration

slick_queue is downloaded automatically during the build process from https://github.com/SlickTech/slick_queue.

## Header-Only Benefits

Being a header-only library provides several advantages:

- **Zero Linking**: No need to link against library files
- **Easy Integration**: Just include the headers in your project
- **Template Optimization**: Compiler can better optimize template instantiations
- **No Binary Dependencies**: No need to distribute or manage .lib/.a files
- **Immediate Usage**: Start logging with just two `#include` statements

## Custom Sinks

Creating custom sinks is straightforward - just inherit from `ISink`:

```cpp
class JsonSink : public slick_logger::ISink {
    std::ofstream file_;
    bool first_entry_ = true;
    
public:
    explicit JsonSink(const std::filesystem::path& filename) : file_(filename) {
        file_ << "[\n"; // Start JSON array
    }
    
    void write(const slick_logger::LogEntry& entry) override {
        // Format as JSON - see examples/multi_sink_example.cpp for full implementation
        const char* level_str = /* convert level to string */;
        std::string message = entry.formatter();
        
        if (!first_entry_) file_ << ",\n";
        first_entry_ = false;
        
        file_ << "  {\n"
              << "    \"timestamp\": \"" << /* formatted timestamp */ << "\",\n"  
              << "    \"level\": \"" << level_str << "\",\n"
              << "    \"message\": \"" << message << "\"\n"
              << "  }";
    }
    
    void flush() override { file_.flush(); }
};

// Usage
Logger::instance().add_sink(std::make_shared<JsonSink>("app.json"));
```

## Examples

The repository includes comprehensive examples:

- **`logger_example.exe`**: Basic usage with console + file output
- **`multi_sink_example.exe`**: Demonstrates all sink types, rotation, and custom sinks

## Building Examples/Tests  

If you want to build the provided examples and tests:

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Debug

# Run examples
./examples/Debug/logger_example.exe
./examples/Debug/multi_sink_example.exe

# Run tests  
./tests/Debug/slick_logger_tests.exe         # Original tests
./tests/Debug/slick_logger_sink_tests.exe    # Multi-sink tests
```
