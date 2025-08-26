#pragma once

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#undef min
#undef max
#endif

#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>
#include <thread>
#include <random>
#include <filesystem>
#include <mutex>
#include <condition_variable>
#include <sstream>

namespace benchmark_utils {

// High-resolution timer for accurate measurements
class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    Timer() { reset(); }

    void reset() {
        start_time_ = Clock::now();
    }

    double elapsed_ms() const {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_time_);
        return duration.count() / 1000000.0;
    }

    double elapsed_us() const {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_time_);
        return duration.count() / 1000.0;
    }

    uint64_t elapsed_ns() const {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_time_);
        return duration.count();
    }

private:
    TimePoint start_time_;
};

// Statistical analysis for benchmark results
class Statistics {
public:
    Statistics(std::vector<double> data) : data_(std::move(data)) {
        std::sort(data_.begin(), data_.end());
        calculate_stats();
    }

    double mean() const { return mean_; }
    double median() const { return median_; }
    double min() const { return data_.empty() ? 0.0 : data_.front(); }
    double max() const { return data_.empty() ? 0.0 : data_.back(); }
    double std_dev() const { return std_dev_; }
    double percentile(double p) const {
        if (data_.empty()) return 0.0;
        size_t index = static_cast<size_t>((p / 100.0) * (data_.size() - 1));
        return data_[std::min<size_t>(index, data_.size() - 1)];
    }

    void print_summary(const std::string& name) const {
        std::cout << "=== " << name << " ===" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Samples: " << data_.size() << std::endl;
        std::cout << "Mean:    " << mean_ << std::endl;
        std::cout << "Median:  " << median_ << std::endl;
        std::cout << "Min:     " << min() << std::endl;
        std::cout << "Max:     " << max() << std::endl;
        std::cout << "StdDev:  " << std_dev_ << std::endl;
        std::cout << "P95:     " << percentile(95) << std::endl;
        std::cout << "P99:     " << percentile(99) << std::endl;
        std::cout << "P99.9:   " << percentile(99.9) << std::endl;
        std::cout << std::endl;
    }

private:
    void calculate_stats() {
        if (data_.empty()) {
            mean_ = median_ = std_dev_ = 0.0;
            return;
        }

        mean_ = std::accumulate(data_.begin(), data_.end(), 0.0) / data_.size();
        
        size_t mid = data_.size() / 2;
        if (data_.size() % 2 == 0) {
            median_ = (data_[mid - 1] + data_[mid]) / 2.0;
        } else {
            median_ = data_[mid];
        }

        double variance = 0.0;
        for (double value : data_) {
            variance += (value - mean_) * (value - mean_);
        }
        variance /= data_.size();
        std_dev_ = std::sqrt(variance);
    }

    std::vector<double> data_;
    double mean_;
    double median_;
    double std_dev_;
};

// Benchmark configuration
struct BenchmarkConfig {
    size_t warmup_iterations = 1000;
    size_t measurement_iterations = 10000;
    size_t num_runs = 5;
    size_t num_threads = 1;
    bool enable_detailed_output = false;
    std::string output_file = "benchmark_output.txt";
};

// Message generator for realistic test data
class MessageGenerator {
public:
    MessageGenerator() : rng_(std::random_device{}()), dist_(0, 1000000) {
        // Pre-generate some test strings of different sizes
        small_messages_ = {
            "Info message",
            "Debug trace",
            "Warning occurred",
            "Error detected",
            "Fatal system failure"
        };

        medium_messages_ = {
            "Processing user request with ID {} at timestamp {} with status {}",
            "Database query executed in {} ms with {} rows returned for table {}",
            "Network request to {} completed with status code {} in {} ms",
            "File operation {} on path {} completed successfully in {} ms",
            "Cache hit rate is {}% for key {} with expiration time {}"
        };

        large_messages_ = {
            "Detailed system report: CPU usage is {}%, memory usage is {} MB out of {} MB total, "
            "disk usage is {} GB out of {} GB total, network throughput is {} Mbps, "
            "active connections: {}, pending requests: {}, cache hit ratio: {}%, "
            "database connections: {}/100, queue depth: {}, last error: {} at timestamp {}",
            
            "Transaction processing report: Transaction ID {} processed {} items totaling ${} "
            "for customer {} at location {} using payment method {} with confirmation {} "
            "processed by system {} on thread {} with priority {} taking {} ms to complete "
            "with validation status {} and audit trail {} stored in database partition {}",
        };
    }

    std::string generate_small() {
        std::uniform_int_distribution<> idx_dist(0, small_messages_.size() - 1);
        return small_messages_[idx_dist(rng_)];
    }

    std::string generate_medium() {
        std::uniform_int_distribution<> idx_dist(0, medium_messages_.size() - 1);
        return medium_messages_[idx_dist(rng_)];
    }

    std::string generate_large() {
        std::uniform_int_distribution<> idx_dist(0, large_messages_.size() - 1);
        return large_messages_[idx_dist(rng_)];
    }

    // Generate random values for formatting
    int random_int() { return dist_(rng_); }
    double random_double() { return dist_(rng_) / 1000.0; }
    std::string random_string() { 
        return "str_" + std::to_string(dist_(rng_)); 
    }

private:
    std::mt19937 rng_;
    std::uniform_int_distribution<int> dist_;
    std::vector<std::string> small_messages_;
    std::vector<std::string> medium_messages_;
    std::vector<std::string> large_messages_;
};

// CPU warming utility to ensure consistent benchmark conditions
class CPUWarmer {
public:
    static void warm_up(std::chrono::milliseconds duration = std::chrono::milliseconds(100)) {
        auto start = std::chrono::high_resolution_clock::now();
        volatile int dummy = 0;
        
        while (std::chrono::high_resolution_clock::now() - start < duration) {
            // Simple CPU-intensive loop
            for (int i = 0; i < 1000; ++i) {
                dummy += i * i;
            }
        }
        
        // Prevent compiler optimization
        static volatile int sink = dummy;
        (void)sink;
    }
};

// Thread barrier for synchronized multi-threaded benchmarks
class ThreadBarrier {
public:
    explicit ThreadBarrier(size_t num_threads) 
        : num_threads_(num_threads), count_(0), generation_(0) {}

    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        size_t gen = generation_;
        
        if (++count_ == num_threads_) {
            count_ = 0;
            ++generation_;
            cv_.notify_all();
        } else {
            cv_.wait(lock, [this, gen] { return gen != generation_; });
        }
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    size_t num_threads_;
    size_t count_;
    size_t generation_;
};

// Result formatter for consistent output
class ResultFormatter {
public:
    static void print_header(const std::string& test_name) {
        std::cout << std::string(80, '=') << std::endl;
        std::cout << "BENCHMARK: " << test_name << std::endl;
        std::cout << std::string(80, '=') << std::endl;
    }

    static void print_comparison_table(const std::vector<std::pair<std::string, Statistics>>& results,
                                     const std::string& unit = "ops/sec") {
        std::cout << std::left << std::setw(20) << "Library" 
                  << std::right << std::setw(12) << "Mean" 
                  << std::right << std::setw(12) << "Median"
                  << std::right << std::setw(12) << "P95"
                  << std::right << std::setw(12) << "P99"
                  << std::right << std::setw(12) << "StdDev" << std::endl;
        std::cout << std::string(80, '-') << std::endl;

        for (const auto& [name, stats] : results) {
            std::cout << std::left << std::setw(20) << name
                      << std::right << std::setw(12) << std::fixed << std::setprecision(0) << stats.mean()
                      << std::right << std::setw(12) << std::fixed << std::setprecision(0) << stats.median()
                      << std::right << std::setw(12) << std::fixed << std::setprecision(0) << stats.percentile(95)
                      << std::right << std::setw(12) << std::fixed << std::setprecision(0) << stats.percentile(99)
                      << std::right << std::setw(12) << std::fixed << std::setprecision(1) << stats.std_dev()
                      << std::endl;
        }
        std::cout << std::endl;
        std::cout << "Unit: " << unit << std::endl;
        std::cout << std::endl;
    }
};

// File utilities for benchmark setup
class FileUtils {
public:
    static void cleanup_test_files() {
        try {
            std::filesystem::remove_all("benchmark_logs");
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    static void create_test_directory() {
        try {
            std::filesystem::create_directories("benchmark_logs");
        } catch (...) {
            // Directory might already exist
        }
    }

    static std::string get_unique_filename(const std::string& prefix) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << "benchmark_logs/" << prefix << "_" 
            << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
            << "_" << std::setfill('0') << std::setw(3) << ms.count()
            << ".log";
        return oss.str();
    }
};

} // namespace benchmark_utils