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
#include <memory>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <cstdint>
#include <functional>

namespace slick_logger {

enum class LogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERR = 4,
    FATAL = 5,
};

struct LogEntry {
    LogLevel level;
    std::function<std::string()> formatter; // Lambda that returns formatted message
    uint64_t timestamp; // nanoseconds since epoch
};

class ISink {
public:
    virtual ~ISink() = default;
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() = 0;
};

struct RotationConfig {
    size_t max_file_size = 10 * 1024 * 1024; // 10MB
    size_t max_files = 5;
    bool compress_old = false;
    std::chrono::hours rotation_hour = std::chrono::hours(0); // Daily at midnight
};

class ConsoleSink : public ISink {
public:
    explicit ConsoleSink(bool use_colors = true, bool use_stderr_for_errors = true);
    
    void write(const LogEntry& entry) override;
    void flush() override;

private:
    std::string format_log_entry(const LogEntry& entry);
    std::string get_color_code(LogLevel level);
    std::string get_reset_code();
    
    bool use_colors_;
    bool use_stderr_for_errors_;
};

class FileSink : public ISink {
public:
    explicit FileSink(const std::filesystem::path& file_path);
    
    void write(const LogEntry& entry) override;
    void flush() override;

protected:
    std::string format_log_entry(const LogEntry& entry);
    
    std::filesystem::path file_path_;
    std::ofstream file_stream_;
};

class RotatingFileSink : public FileSink {
public:
    RotatingFileSink(const std::filesystem::path& base_path, const RotationConfig& config);
    
    void write(const LogEntry& entry) override;

private:
    void check_rotation();
    void rotate_files();
    std::filesystem::path get_rotated_filename(size_t index);
    
    RotationConfig config_;
    std::filesystem::path base_path_;
    size_t current_file_size_;
};

class DailyFileSink : public FileSink {
public:
    DailyFileSink(const std::filesystem::path& base_path, const RotationConfig& config);
    
    void write(const LogEntry& entry) override;

private:
    void check_rotation();
    std::filesystem::path get_daily_filename();
    std::string get_date_string();
    
    RotationConfig config_;
    std::filesystem::path base_path_;
    std::string current_date_;
};

// Implementation (header-only library)

inline ConsoleSink::ConsoleSink(bool use_colors, bool use_stderr_for_errors)
    : use_colors_(use_colors), use_stderr_for_errors_(use_stderr_for_errors) {
}

inline void ConsoleSink::write(const LogEntry& entry) {
    std::string formatted = format_log_entry(entry);
    
    if (use_stderr_for_errors_ && (entry.level >= LogLevel::WARN)) {
        std::cerr << formatted << std::endl;
    } else {
        std::cout << formatted << std::endl;
    }
}

inline void ConsoleSink::flush() {
    std::cout.flush();
    std::cerr.flush();
}

inline std::string ConsoleSink::format_log_entry(const LogEntry& entry) {
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
    std::string result = std::string(time_str) + " [" + level_str + "] " + message;
    
    if (use_colors_) {
        return get_color_code(entry.level) + result + get_reset_code();
    }
    
    return result;
}

inline std::string ConsoleSink::get_color_code(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "\033[90m";   // Dark gray
        case LogLevel::DEBUG: return "\033[36m";   // Cyan
        case LogLevel::INFO: return "\033[32m";    // Green
        case LogLevel::WARN: return "\033[33m";    // Yellow
        case LogLevel::ERR: return "\033[31m";     // Red
        case LogLevel::FATAL: return "\033[91m";   // Bright red
        default: return "";
    }
}

inline std::string ConsoleSink::get_reset_code() {
    return "\033[0m";
}

inline FileSink::FileSink(const std::filesystem::path& file_path)
    : file_path_(file_path) {
    file_stream_.open(file_path_, std::ios::app);
    if (!file_stream_) {
        throw std::runtime_error("Failed to open log file: " + file_path_.string());
    }
}

inline void FileSink::write(const LogEntry& entry) {
    if (file_stream_) {
        file_stream_ << format_log_entry(entry) << std::endl;
    }
}

inline void FileSink::flush() {
    if (file_stream_) {
        file_stream_.flush();
    }
}

inline std::string FileSink::format_log_entry(const LogEntry& entry) {
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
    return std::string(time_str) + " [" + level_str + "] " + message;
}

inline RotatingFileSink::RotatingFileSink(const std::filesystem::path& base_path, const RotationConfig& config)
    : FileSink(base_path), config_(config), base_path_(base_path), current_file_size_(0) {
    if (std::filesystem::exists(base_path_)) {
        current_file_size_ = std::filesystem::file_size(base_path_);
    }
}

inline void RotatingFileSink::write(const LogEntry& entry) {
    check_rotation();
    
    std::string formatted = format_log_entry(entry);
    if (file_stream_) {
        file_stream_ << formatted << std::endl;
        current_file_size_ += formatted.length() + 1; // +1 for newline
    }
}

inline void RotatingFileSink::check_rotation() {
    if (current_file_size_ >= config_.max_file_size) {
        rotate_files();
    }
}

inline void RotatingFileSink::rotate_files() {
    file_stream_.close();
    
    // Remove the oldest file if it exists
    auto oldest_file = get_rotated_filename(config_.max_files - 1);
    if (std::filesystem::exists(oldest_file)) {
        std::filesystem::remove(oldest_file);
    }
    
    // Rotate existing files
    for (size_t i = config_.max_files - 1; i > 0; --i) {
        auto src = (i == 1) ? base_path_ : get_rotated_filename(i - 1);
        auto dst = get_rotated_filename(i);
        
        if (std::filesystem::exists(src)) {
            std::filesystem::rename(src, dst);
        }
    }
    
    // Create new current file
    file_stream_.open(base_path_, std::ios::out | std::ios::trunc);
    current_file_size_ = 0;
}

inline std::filesystem::path RotatingFileSink::get_rotated_filename(size_t index) {
    std::string filename = base_path_.stem().string() + "_" + std::to_string(index) + base_path_.extension().string();
    return base_path_.parent_path() / filename;
}

inline DailyFileSink::DailyFileSink(const std::filesystem::path& base_path, const RotationConfig& config)
    : FileSink(base_path), config_(config), base_path_(base_path) {
    current_date_ = get_date_string();
    
    // Close the base file and reopen with the daily filename
    file_stream_.close();
    file_path_ = get_daily_filename();
    file_stream_.open(file_path_, std::ios::app);
    if (!file_stream_) {
        throw std::runtime_error("Failed to open daily log file: " + file_path_.string());
    }
}

inline void DailyFileSink::write(const LogEntry& entry) {
    check_rotation();
    FileSink::write(entry);
}

inline void DailyFileSink::check_rotation() {
    std::string today = get_date_string();
    if (today != current_date_) {
        file_stream_.close();
        file_path_ = get_daily_filename();
        file_stream_.open(file_path_, std::ios::app);
        current_date_ = today;
    }
}

inline std::filesystem::path DailyFileSink::get_daily_filename() {
    std::string date_str = get_date_string();
    std::string filename = base_path_.stem().string() + "_" + date_str + base_path_.extension().string();
    return base_path_.parent_path() / filename;
}

inline std::string DailyFileSink::get_date_string() {
    auto now = std::chrono::system_clock::now();
    time_t time_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_val);
    
    char date_str[11];
    std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);
    return std::string(date_str);
}

} // namespace slick_logger