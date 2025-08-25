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
#include <slick_queue.h>
#include "version.hpp"

namespace slick_logger {

enum class LogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERR = 4,
    FATAL = 5
};

struct LogEntry {
    LogLevel level;
    std::function<std::string()> formatter; // Lambda that returns formatted message
    uint64_t timestamp; // nanoseconds since epoch
};

class Logger {
public:
    static Logger& instance();

    void init(const std::filesystem::path& log_file, size_t queue_size = 65536);

    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args&&... args);

    void shutdown();

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void writer_thread_func();

    slick::SlickQueue<LogEntry>* queue_;
    std::filesystem::path log_file_;
    std::thread writer_thread_;
    std::atomic<bool> running_{false};
    uint64_t read_index_{0};
};

// Implementation (header-only library)

inline Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

inline void Logger::init(const std::filesystem::path& log_file, size_t queue_size) {
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

template<typename... Args>
inline void Logger::log(LogLevel level, const std::string& format, Args&&... args) {
    if (!running_ || !queue_) return;

    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    // Create lambda that captures format and arguments, formats when called
    auto formatter = [format, args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable -> std::string {
        return std::apply([&format](auto&&... unpacked_args) -> std::string {
            return std::vformat(format, std::make_format_args(std::forward<decltype(unpacked_args)>(unpacked_args)...));
        }, args_tuple);
    };

    LogEntry entry{level, std::move(formatter), static_cast<uint64_t>(ns)};

    uint64_t index = queue_->reserve();
    *(*queue_)[index] = entry;
    queue_->publish(index);
}

inline void Logger::shutdown() {
    if (!running_) return;
    running_ = false;
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
    delete queue_;
    queue_ = nullptr;
}

inline Logger::~Logger() {
    shutdown();
}

inline void Logger::writer_thread_func() {
    std::ofstream log_stream(log_file_, std::ios::app);
    if (!log_stream) {
        std::cerr << "Failed to open log file: " << log_file_ << std::endl;
        return;
    }

    while (running_) {
        auto [entry_ptr, count] = queue_->read(read_index_);
        if (entry_ptr) {
            for (uint32_t i = 0; i < count; ++i) {
                const LogEntry& entry = entry_ptr[i];
                time_t time_val = static_cast<time_t>(entry.timestamp / 1000000000); // Convert nanoseconds to seconds
                std::tm tm = *std::localtime(&time_val);

                std::string level_str;
                switch (entry.level) {
                    case LogLevel::TRACE: level_str = "TRACE"; break;
                    case LogLevel::DEBUG: level_str = "DEBUG"; break;
                    case LogLevel::INFO: level_str = "INFO"; break;
                    case LogLevel::WARN: level_str = "WARN"; break;
                    case LogLevel::ERR: level_str = "ERROR"; break;
                    case LogLevel::FATAL: level_str = "FATAL"; break;
                }

                char time_str[20];
                std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);

                // Format the message in the background thread
                std::string message = entry.formatter();
                log_stream << time_str << " [" << level_str << "] " << message << std::endl;
            }
            log_stream.flush();
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
        for (uint32_t i = 0; i < count; ++i) {
            const LogEntry& entry = entry_ptr[i];
            time_t time_val = static_cast<time_t>(entry.timestamp / 1000000000);
            std::tm tm = *std::localtime(&time_val);

            std::string level_str;
            switch (entry.level) {
                case LogLevel::TRACE: level_str = "TRACE"; break;
                case LogLevel::DEBUG: level_str = "DEBUG"; break;
                case LogLevel::INFO: level_str = "INFO"; break;
                case LogLevel::WARN: level_str = "WARN"; break;
                case LogLevel::ERR: level_str = "ERROR"; break;
                case LogLevel::FATAL: level_str = "FATAL"; break;
            }

            char time_str[20];
            std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);

            std::string message = entry.formatter();
            log_stream << time_str << " [" << level_str << "] " << message << std::endl;
        }
        log_stream.flush();
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
