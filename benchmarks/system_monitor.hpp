#pragma once

#include <chrono>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #pragma comment(lib, "psapi.lib")
#else
    #include <sys/resource.h>
    #include <fstream>
    #include <sstream>
    #include <unistd.h>
#endif

namespace benchmark_utils {

// System resource usage snapshot
struct ResourceUsage {
    double cpu_percent = 0.0;
    size_t memory_bytes = 0;
    size_t memory_peak_bytes = 0;
    double elapsed_time_ms = 0.0;
    
    void print() const {
        std::cout << "Resource Usage:" << std::endl;
        std::cout << "  CPU:         " << std::fixed << std::setprecision(1) << cpu_percent << "%" << std::endl;
        std::cout << "  Memory:      " << (memory_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
        std::cout << "  Peak Memory: " << (memory_peak_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
        std::cout << "  Time:        " << elapsed_time_ms << " ms" << std::endl;
        std::cout << std::endl;
    }
};

// Cross-platform system resource monitor
class SystemMonitor {
public:
    SystemMonitor() : monitoring_(false), peak_memory_(0) {
#ifdef _WIN32
        process_handle_ = GetCurrentProcess();
#endif
        baseline_memory_ = get_current_memory_usage();
    }

    ~SystemMonitor() {
        stop_monitoring();
    }

    void start_monitoring() {
        if (monitoring_.exchange(true)) {
            return; // Already monitoring
        }

        start_time_ = std::chrono::high_resolution_clock::now();
        peak_memory_ = baseline_memory_;
        
#ifdef _WIN32
        // Get initial CPU times for calculating usage
        FILETIME creation_time, exit_time, kernel_time, user_time;
        GetProcessTimes(process_handle_, &creation_time, &exit_time, &kernel_time, &user_time);
        
        last_kernel_time_ = filetime_to_uint64(kernel_time);
        last_user_time_ = filetime_to_uint64(user_time);
        last_check_time_ = std::chrono::high_resolution_clock::now();
#endif

        // Start monitoring thread
        monitor_thread_ = std::thread([this] { monitor_loop(); });
    }

    void stop_monitoring() {
        if (!monitoring_.exchange(false)) {
            return; // Not monitoring
        }

        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }

        end_time_ = std::chrono::high_resolution_clock::now();
    }

    ResourceUsage get_current_usage() {
        ResourceUsage usage;
        
        usage.memory_bytes = get_current_memory_usage() - baseline_memory_;
        usage.memory_peak_bytes = peak_memory_ - baseline_memory_;
        
        if (monitoring_) {
            auto now = std::chrono::high_resolution_clock::now();
            usage.elapsed_time_ms = std::chrono::duration<double, std::milli>(now - start_time_).count();
        } else {
            usage.elapsed_time_ms = std::chrono::duration<double, std::milli>(end_time_ - start_time_).count();
        }

#ifdef _WIN32
        usage.cpu_percent = get_cpu_usage();
#endif

        return usage;
    }

    void reset() {
        stop_monitoring();
        baseline_memory_ = get_current_memory_usage();
        peak_memory_ = baseline_memory_;
    }

private:
    void monitor_loop() {
        while (monitoring_) {
            size_t current_memory = get_current_memory_usage();
            if (current_memory > peak_memory_) {
                peak_memory_ = current_memory;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    size_t get_current_memory_usage() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(process_handle_, &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize;
        }
        return 0;
#else
        // Linux/Unix implementation
        std::ifstream status_file("/proc/self/status");
        if (!status_file.is_open()) {
            return 0;
        }

        std::string line;
        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string label;
                size_t value;
                std::string unit;
                
                if (iss >> label >> value >> unit) {
                    // Convert from KB to bytes
                    return value * 1024;
                }
                break;
            }
        }
        return 0;
#endif
    }

#ifdef _WIN32
    static uint64_t filetime_to_uint64(const FILETIME& ft) {
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        return uli.QuadPart;
    }

    double get_cpu_usage() {
        FILETIME creation_time, exit_time, kernel_time, user_time;
        if (!GetProcessTimes(process_handle_, &creation_time, &exit_time, &kernel_time, &user_time)) {
            return 0.0;
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto wall_time_diff = std::chrono::duration<double>(now - last_check_time_).count() * 10000000; // Convert to 100ns units

        uint64_t current_kernel = filetime_to_uint64(kernel_time);
        uint64_t current_user = filetime_to_uint64(user_time);

        uint64_t kernel_diff = current_kernel - last_kernel_time_;
        uint64_t user_diff = current_user - last_user_time_;
        uint64_t total_cpu_diff = kernel_diff + user_diff;

        last_kernel_time_ = current_kernel;
        last_user_time_ = current_user;
        last_check_time_ = now;

        if (wall_time_diff > 0) {
            return (double(total_cpu_diff) / wall_time_diff) * 100.0;
        }
        return 0.0;
    }

    HANDLE process_handle_;
    uint64_t last_kernel_time_ = 0;
    uint64_t last_user_time_ = 0;
    std::chrono::high_resolution_clock::time_point last_check_time_;
#endif

    std::atomic<bool> monitoring_;
    std::thread monitor_thread_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    size_t baseline_memory_;
    std::atomic<size_t> peak_memory_;
};

// RAII wrapper for automatic monitoring
class ScopedMonitor {
public:
    explicit ScopedMonitor(SystemMonitor& monitor) : monitor_(monitor) {
        monitor_.start_monitoring();
    }

    ~ScopedMonitor() {
        monitor_.stop_monitoring();
    }

    ResourceUsage get_usage() {
        return monitor_.get_current_usage();
    }

private:
    SystemMonitor& monitor_;
};

// Memory leak detector for benchmarks
class MemoryLeakDetector {
public:
    MemoryLeakDetector() {
        initial_memory_ = get_memory_usage();
    }

    ~MemoryLeakDetector() {
        check_for_leaks();
    }

    void check_for_leaks() {
        size_t final_memory = get_memory_usage();
        size_t leaked_bytes = final_memory > initial_memory_ ? 
                             final_memory - initial_memory_ : 0;
        
        if (leaked_bytes > threshold_bytes_) {
            std::cout << "WARNING: Potential memory leak detected!" << std::endl;
            std::cout << "Memory increased by " << (leaked_bytes / 1024.0 / 1024.0) 
                      << " MB during benchmark" << std::endl;
        }
    }

    void set_leak_threshold(size_t threshold_bytes) {
        threshold_bytes_ = threshold_bytes;
    }

private:
    size_t get_memory_usage() {
        SystemMonitor monitor;
        return monitor.get_current_usage().memory_bytes;
    }

    size_t initial_memory_;
    size_t threshold_bytes_ = 1024 * 1024; // 1MB default threshold
};

} // namespace benchmark_utils