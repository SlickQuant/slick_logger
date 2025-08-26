// MIT License
//
// Copyright (c) 2025 SlickTech
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#pragma once

#include <string>
#include <cstdint>
#include <thread>
#include <atomic>
#include <filesystem>
#include <memory>
#include <functional>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <format>
#include <utility>
#include <vector>
#include <slick_queue.h>
#include "version.hpp"
#include "sinks.hpp"

namespace slick_logger {

struct LogConfig {
    std::vector<std::shared_ptr<ISink>> sinks;
    LogLevel min_level = LogLevel::TRACE;
    size_t queue_size = 65536;
};

class Logger {
public:
    static Logger& instance();

    void init(const std::filesystem::path& log_file, size_t queue_size = 65536);
    void init(const LogConfig& config);
    void init(size_t queue_size = 65536);
    
    void add_sink(std::shared_ptr<ISink> sink);
    void clear_sinks();
    
    void add_console_sink(bool use_colors = true, bool use_stderr_for_errors = true);
    void add_console_sink(TimestampFormatter::Format timestamp_format, bool use_colors = true, bool use_stderr_for_errors = true);
    void add_console_sink(const std::string& custom_timestamp_format, bool use_colors = true, bool use_stderr_for_errors = true);
    
    void add_file_sink(const std::filesystem::path& path, const RotationConfig& config = {});
    void add_file_sink(const std::filesystem::path& path, TimestampFormatter::Format timestamp_format, const RotationConfig& config = {});
    void add_file_sink(const std::filesystem::path& path, const std::string& custom_timestamp_format, const RotationConfig& config = {});
    
    void add_rotating_file_sink(const std::filesystem::path& path, const RotationConfig& config);
    void add_rotating_file_sink(const std::filesystem::path& path, const RotationConfig& config, TimestampFormatter::Format timestamp_format);
    void add_rotating_file_sink(const std::filesystem::path& path, const RotationConfig& config, const std::string& custom_timestamp_format);
    
    void add_daily_file_sink(const std::filesystem::path& path, const RotationConfig& config = {});
    void add_daily_file_sink(const std::filesystem::path& path, const RotationConfig& config, TimestampFormatter::Format timestamp_format);
    void add_daily_file_sink(const std::filesystem::path& path, const RotationConfig& config, const std::string& custom_timestamp_format);

    void set_log_level(LogLevel level) {
        log_level_.store(level, std::memory_order_release);
    }

    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args&&... args);

    void shutdown();
    void reset(); // For testing - clears everything and allows reinitialization

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void writer_thread_func();
    void write_log_entry(const LogEntry* entry_ptr, uint32_t count);

    slick::SlickQueue<LogEntry>* queue_;
    std::vector<std::shared_ptr<ISink>> sinks_;
    std::filesystem::path log_file_;
    std::thread writer_thread_;
    std::atomic<bool> running_{false};
    uint64_t read_index_{0};
    std::atomic<LogLevel> log_level_{LogLevel::TRACE};
};

// Implementation (header-only library)

inline Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

inline void Logger::init(const std::filesystem::path& log_file, size_t queue_size) {
    // Backwards compatibility - create file sink for the specified file
    clear_sinks();
    add_sink(std::make_shared<FileSink>(log_file));
    
    // Ensure queue_size is power of 2
    if (queue_size & (queue_size - 1)) {
        // Round up to next power of 2
        size_t temp = queue_size;
        temp--;
        temp |= temp >> 1;
        temp |= temp >> 2;
        temp |= temp >> 4;
        temp |= temp >> 8;
        temp |= temp >> 16;
        temp |= temp >> 32;
        queue_size = temp + 1;
    }

    queue_ = new slick::SlickQueue<LogEntry>(static_cast<uint32_t>(queue_size));
    log_file_ = log_file;
    running_ = true;
    
    // Initialize read_index_ before starting the thread
    read_index_ = queue_->initial_reading_index();
    
    writer_thread_ = std::thread([this]() { writer_thread_func(); });
    
    // Give a small delay to ensure writer thread is started
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

inline void Logger::init(const LogConfig& config) {
    clear_sinks();
    
    for (auto& sink : config.sinks) {
        add_sink(sink);
    }
    
    set_log_level(config.min_level);
    
    // Ensure queue_size is power of 2
    size_t queue_size = config.queue_size;
    if (queue_size & (queue_size - 1)) {
        // Round up to next power of 2
        size_t temp = queue_size;
        temp--;
        temp |= temp >> 1;
        temp |= temp >> 2;
        temp |= temp >> 4;
        temp |= temp >> 8;
        temp |= temp >> 16;
        temp |= temp >> 32;
        queue_size = temp + 1;
    }

    queue_ = new slick::SlickQueue<LogEntry>(static_cast<uint32_t>(queue_size));
    running_ = true;
    
    // Initialize read_index_ before starting the thread
    read_index_ = queue_->initial_reading_index();
    
    writer_thread_ = std::thread([this]() { writer_thread_func(); });
    
    // Give a small delay to ensure writer thread is started
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

inline void Logger::add_sink(std::shared_ptr<ISink> sink) {
    sinks_.push_back(sink);
}

inline void Logger::clear_sinks() {
    sinks_.clear();
}

inline void Logger::init(size_t queue_size) {
    // Initialize logger with empty sinks - sinks should be added before calling this
    // Ensure queue_size is power of 2
    if (queue_size & (queue_size - 1)) {
        // Round up to next power of 2
        size_t temp = queue_size;
        temp--;
        temp |= temp >> 1;
        temp |= temp >> 2;
        temp |= temp >> 4;
        temp |= temp >> 8;
        temp |= temp >> 16;
        temp |= temp >> 32;
        queue_size = temp + 1;
    }

    queue_ = new slick::SlickQueue<LogEntry>(static_cast<uint32_t>(queue_size));
    running_ = true;
    
    // Initialize read_index_ before starting the thread
    read_index_ = queue_->initial_reading_index();
    
    writer_thread_ = std::thread([this]() { writer_thread_func(); });
    
    // Give a small delay to ensure writer thread is started
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

inline void Logger::add_console_sink(bool use_colors, bool use_stderr_for_errors) {
    add_sink(std::make_shared<ConsoleSink>(use_colors, use_stderr_for_errors));
}

inline void Logger::add_console_sink(TimestampFormatter::Format timestamp_format, bool use_colors, bool use_stderr_for_errors) {
    add_sink(std::make_shared<ConsoleSink>(use_colors, use_stderr_for_errors, timestamp_format));
}

inline void Logger::add_console_sink(const std::string& custom_timestamp_format, bool use_colors, bool use_stderr_for_errors) {
    add_sink(std::make_shared<ConsoleSink>(custom_timestamp_format, use_colors, use_stderr_for_errors));
}

inline void Logger::add_file_sink(const std::filesystem::path& path, const RotationConfig& config) {
    add_sink(std::make_shared<FileSink>(path));
}

inline void Logger::add_file_sink(const std::filesystem::path& path, TimestampFormatter::Format timestamp_format, const RotationConfig& config) {
    add_sink(std::make_shared<FileSink>(path, timestamp_format));
}

inline void Logger::add_file_sink(const std::filesystem::path& path, const std::string& custom_timestamp_format, const RotationConfig& config) {
    add_sink(std::make_shared<FileSink>(path, custom_timestamp_format));
}

inline void Logger::add_rotating_file_sink(const std::filesystem::path& path, const RotationConfig& config) {
    add_sink(std::make_shared<RotatingFileSink>(path, config));
}

inline void Logger::add_rotating_file_sink(const std::filesystem::path& path, const RotationConfig& config, TimestampFormatter::Format timestamp_format) {
    add_sink(std::make_shared<RotatingFileSink>(path, config, timestamp_format));
}

inline void Logger::add_rotating_file_sink(const std::filesystem::path& path, const RotationConfig& config, const std::string& custom_timestamp_format) {
    add_sink(std::make_shared<RotatingFileSink>(path, config, custom_timestamp_format));
}

inline void Logger::add_daily_file_sink(const std::filesystem::path& path, const RotationConfig& config) {
    add_sink(std::make_shared<DailyFileSink>(path, config));
}

inline void Logger::add_daily_file_sink(const std::filesystem::path& path, const RotationConfig& config, TimestampFormatter::Format timestamp_format) {
    add_sink(std::make_shared<DailyFileSink>(path, config, timestamp_format));
}

inline void Logger::add_daily_file_sink(const std::filesystem::path& path, const RotationConfig& config, const std::string& custom_timestamp_format) {
    add_sink(std::make_shared<DailyFileSink>(path, config, custom_timestamp_format));
}

template<typename... Args>
inline void Logger::log(LogLevel level, const std::string& format, Args&&... args) {
    if (!running_ || !queue_ || level < log_level_.load(std::memory_order_relaxed))
    {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    // Create lambda that captures format and arguments, formats when called
    auto formatter = [format, args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable -> std::string {
        return std::apply([&format](auto&&... unpacked_args) -> std::string {
            return std::vformat(format, std::make_format_args(std::forward<decltype(unpacked_args)>(unpacked_args)...));
        }, args_tuple);
    };

    uint64_t index = queue_->reserve();
    auto &entry_ref = *(*queue_)[index];
    entry_ref.level = level;
    entry_ref.formatter = std::move(formatter);
    entry_ref.timestamp = static_cast<uint64_t>(ns);
    queue_->publish(index);
}

inline void Logger::shutdown() {
    if (!running_) return;
    running_ = false;
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
    
    // Clear sinks to release file handles and other resources
    sinks_.clear();
    
    delete queue_;
    queue_ = nullptr;
}

inline Logger::~Logger() {
    shutdown();
}

inline void Logger::reset() {
    shutdown();
    // Reset all state for fresh initialization
    log_file_.clear();
    read_index_ = 0;
    log_level_.store(LogLevel::TRACE);
}

inline void Logger::writer_thread_func() {
    while (running_) {
        auto [entry_ptr, count] = queue_->read(read_index_);
        if (entry_ptr) {
            write_log_entry(entry_ptr, count);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Small delay if no data
        }
    }
    
    // Drain remaining messages after running_ becomes false
    while (true) {
        auto [entry_ptr, count] = queue_->read(read_index_);
        if (!entry_ptr || count == 0) {
            break;
        }
        write_log_entry(entry_ptr, count);
    }
}

inline void Logger::write_log_entry(const LogEntry* entry_ptr, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        const LogEntry& entry = entry_ptr[i];
        
        // Write to all configured sinks
        for (auto& sink : sinks_) {
            if (sink) {
                sink->write(entry);
            }
        }
    }
    
    // Flush all sinks
    for (auto& sink : sinks_) {
        if (sink) {
            sink->flush();
        }
    }
}

// Macros for easy logging
#define LOG_TRACE(...) slick_logger::Logger::instance().log(slick_logger::LogLevel::TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) slick_logger::Logger::instance().log(slick_logger::LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) slick_logger::Logger::instance().log(slick_logger::LogLevel::INFO, __VA_ARGS__)
#define LOG_WARN(...) slick_logger::Logger::instance().log(slick_logger::LogLevel::WARN, __VA_ARGS__)
#define LOG_ERROR(...) slick_logger::Logger::instance().log(slick_logger::LogLevel::ERR, __VA_ARGS__)
#define LOG_FATAL(...) slick_logger::Logger::instance().log(slick_logger::LogLevel::FATAL, __VA_ARGS__)

} // namespace slick_logger
