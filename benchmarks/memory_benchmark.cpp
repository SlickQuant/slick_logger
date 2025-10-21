#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#include <slick/logger.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>

#include "benchmark_utils.hpp"
#include "system_monitor.hpp"

using namespace benchmark_utils;

// Detailed memory usage analysis for logging libraries
class MemoryAnalyzer {
public:
    struct MemoryProfile {
        std::string logger_name;
        size_t baseline_memory_mb;
        size_t peak_memory_mb;
        size_t final_memory_mb;
        size_t queue_size;
        size_t messages_logged;
        double memory_per_message_bytes;
        double memory_efficiency_score;
    };

    void run_memory_analysis() {
        std::cout << "=== MEMORY USAGE ANALYSIS ===" << std::endl;
        
        std::vector<MemoryProfile> profiles;
        
        // Test different queue sizes
        std::vector<size_t> queue_sizes = {1024, 8192, 65536, 262144};
        
        for (size_t queue_size : queue_sizes) {
            std::cout << "\nTesting with queue size: " << queue_size << std::endl;
            
            profiles.push_back(analyze_slick_logger_memory(queue_size));
            profiles.push_back(analyze_spdlog_async_memory(queue_size));
        }
        
        print_memory_comparison(profiles);
        
        // Test memory under sustained load
        test_sustained_load_memory();
        
        // Test memory fragmentation
        test_memory_fragmentation();
    }

private:
    MemoryProfile analyze_slick_logger_memory(size_t queue_size) {
        SystemMonitor monitor;
        
        // Get baseline memory
        auto baseline_usage = monitor.get_current_usage();
        size_t baseline_memory = baseline_usage.memory_bytes;
        
        // Initialize logger
        slick::logger::Logger::instance().reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Let cleanup complete
        
        monitor.start_monitoring();
        
        slick::logger::Logger::instance().add_file_sink(
            FileUtils::get_unique_filename("slick_memory"));
        slick::logger::Logger::instance().init(queue_size);
        
        // Log test messages
        MessageGenerator msg_gen;
        const size_t num_messages = queue_size * 2; // Fill queue twice
        
        for (size_t i = 0; i < num_messages; ++i) {
            LOG_INFO("Memory test message {} with data {}", i, msg_gen.generate_medium());
            
            // Sample memory usage periodically
            if (i % 1000 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        // Let all messages flush
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        monitor.stop_monitoring();
        auto final_usage = monitor.get_current_usage();
        
        slick::logger::Logger::instance().shutdown();
        
        MemoryProfile profile;
        profile.logger_name = "SlickLogger";
        profile.baseline_memory_mb = baseline_memory / 1024 / 1024;
        profile.peak_memory_mb = final_usage.memory_peak_bytes / 1024 / 1024;
        profile.final_memory_mb = final_usage.memory_bytes / 1024 / 1024;
        profile.queue_size = queue_size;
        profile.messages_logged = num_messages;
        profile.memory_per_message_bytes = 
            double(final_usage.memory_peak_bytes - baseline_memory) / num_messages;
        profile.memory_efficiency_score = 
            double(num_messages) / (final_usage.memory_peak_bytes - baseline_memory) * 1000000;
        
        std::cout << "SlickLogger (queue=" << queue_size << "): Peak=" 
                  << profile.peak_memory_mb << "MB, Per-message=" 
                  << std::fixed << std::setprecision(1) << profile.memory_per_message_bytes 
                  << " bytes" << std::endl;
        
        return profile;
    }

    MemoryProfile analyze_spdlog_async_memory(size_t queue_size) {
        SystemMonitor monitor;
        
        // Get baseline memory
        auto baseline_usage = monitor.get_current_usage();
        size_t baseline_memory = baseline_usage.memory_bytes;
        
        // Initialize spdlog
        spdlog::shutdown();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        monitor.start_monitoring();
        
        spdlog::init_thread_pool(queue_size, 1);
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            FileUtils::get_unique_filename("spdlog_memory"));
        auto logger = std::make_shared<spdlog::async_logger>("spdlog_async", file_sink, 
                                                            spdlog::thread_pool());
        logger->set_level(spdlog::level::info);
        
        // Log test messages
        MessageGenerator msg_gen;
        const size_t num_messages = queue_size * 2;
        
        for (size_t i = 0; i < num_messages; ++i) {
            logger->info("Memory test message {} with data {}", i, msg_gen.generate_medium());
            
            if (i % 1000 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        // Let all messages flush
        logger->flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        monitor.stop_monitoring();
        auto final_usage = monitor.get_current_usage();
        
        spdlog::shutdown();
        
        MemoryProfile profile;
        profile.logger_name = "spdlog_async";
        profile.baseline_memory_mb = baseline_memory / 1024 / 1024;
        profile.peak_memory_mb = final_usage.memory_peak_bytes / 1024 / 1024;
        profile.final_memory_mb = final_usage.memory_bytes / 1024 / 1024;
        profile.queue_size = queue_size;
        profile.messages_logged = num_messages;
        profile.memory_per_message_bytes = 
            double(final_usage.memory_peak_bytes - baseline_memory) / num_messages;
        profile.memory_efficiency_score = 
            double(num_messages) / (final_usage.memory_peak_bytes - baseline_memory) * 1000000;
        
        std::cout << "spdlog async (queue=" << queue_size << "): Peak=" 
                  << profile.peak_memory_mb << "MB, Per-message=" 
                  << std::fixed << std::setprecision(1) << profile.memory_per_message_bytes 
                  << " bytes" << std::endl;
        
        return profile;
    }

    void print_memory_comparison(const std::vector<MemoryProfile>& profiles) {
        std::cout << "\n=== MEMORY USAGE COMPARISON ===" << std::endl;
        std::cout << std::left << std::setw(15) << "Logger"
                  << std::right << std::setw(10) << "Queue Size"
                  << std::right << std::setw(10) << "Peak MB"
                  << std::right << std::setw(12) << "Bytes/Msg"
                  << std::right << std::setw(12) << "Efficiency" << std::endl;
        std::cout << std::string(70, '-') << std::endl;

        for (const auto& profile : profiles) {
            std::cout << std::left << std::setw(15) << profile.logger_name
                      << std::right << std::setw(10) << profile.queue_size
                      << std::right << std::setw(10) << profile.peak_memory_mb
                      << std::right << std::setw(12) << std::fixed << std::setprecision(1) 
                      << profile.memory_per_message_bytes
                      << std::right << std::setw(12) << std::fixed << std::setprecision(0) 
                      << profile.memory_efficiency_score << std::endl;
        }
        std::cout << "\nEfficiency = Messages per MB of memory used" << std::endl;
        std::cout << std::endl;
    }

    void test_sustained_load_memory() {
        std::cout << "=== SUSTAINED LOAD MEMORY TEST ===" << std::endl;
        
        const auto test_duration = std::chrono::seconds(30);
        const size_t messages_per_second = 10000;
        const auto message_interval = std::chrono::nanoseconds(1000000000 / messages_per_second);
        
        std::cout << "Running sustained load test for " << test_duration.count() 
                  << " seconds at " << messages_per_second << " msgs/sec" << std::endl;
        
        // Test SlickLogger
        {
            std::cout << "Testing SlickLogger..." << std::endl;
            
            slick::logger::Logger::instance().reset();
            slick::logger::Logger::instance().add_file_sink(
                FileUtils::get_unique_filename("slick_sustained"));
            slick::logger::Logger::instance().init(65536);

            SystemMonitor monitor;
            monitor.start_monitoring();
            
            std::atomic<bool> running{true};
            std::atomic<size_t> message_count{0};
            
            std::thread logger_thread([&running, &message_count, message_interval]() {
                MessageGenerator msg_gen;
                auto next_message_time = std::chrono::high_resolution_clock::now();
                
                while (running) {
                    LOG_INFO("Sustained load message {} - {}", 
                           message_count.fetch_add(1), msg_gen.generate_small());
                    
                    next_message_time += message_interval;
                    auto now = std::chrono::high_resolution_clock::now();
                    if (now < next_message_time) {
                        std::this_thread::sleep_until(next_message_time);
                    }
                }
            });
            
            std::this_thread::sleep_for(test_duration);
            running = false;
            logger_thread.join();
            
            monitor.stop_monitoring();
            auto usage = monitor.get_current_usage();
            
            std::cout << "SlickLogger sustained load results:" << std::endl;
            std::cout << "  Messages logged: " << message_count.load() << std::endl;
            std::cout << "  Peak memory: " << (usage.memory_peak_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
            std::cout << "  Final memory: " << (usage.memory_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
            std::cout << std::endl;
            
            slick::logger::Logger::instance().shutdown();
        }
    }

    void test_memory_fragmentation() {
        std::cout << "=== MEMORY FRAGMENTATION TEST ===" << std::endl;
        
        // Test repeated initialization/shutdown cycles
        const size_t num_cycles = 10;
        const size_t messages_per_cycle = 10000;
        
        std::vector<size_t> memory_snapshots;
        SystemMonitor monitor;
        
        for (size_t cycle = 0; cycle < num_cycles; ++cycle) {
            slick::logger::Logger::instance().reset();
            slick::logger::Logger::instance().add_file_sink(
                FileUtils::get_unique_filename("slick_frag_" + std::to_string(cycle)));
            slick::logger::Logger::instance().init(8192);
            
            MessageGenerator msg_gen;
            for (size_t i = 0; i < messages_per_cycle; ++i) {
                LOG_INFO("Fragmentation test cycle {} message {} - {}", 
                       cycle, i, msg_gen.generate_small());
            }
            
            slick::logger::Logger::instance().shutdown();
            
            // Small delay to let cleanup complete
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            auto usage = monitor.get_current_usage();
            memory_snapshots.push_back(usage.memory_bytes);
            
            std::cout << "Cycle " << cycle << ": " << (usage.memory_bytes / 1024.0 / 1024.0) 
                      << " MB" << std::endl;
        }
        
        // Analyze memory growth
        size_t initial_memory = memory_snapshots.front();
        size_t final_memory = memory_snapshots.back();
        size_t growth = final_memory > initial_memory ? final_memory - initial_memory : 0;
        
        std::cout << "Memory growth over " << num_cycles << " cycles: " 
                  << (growth / 1024.0 / 1024.0) << " MB" << std::endl;
        
        if (growth < 1024 * 1024) { // Less than 1MB growth
            std::cout << "PASS: Minimal memory fragmentation detected" << std::endl;
        } else {
            std::cout << "WARNING: Significant memory growth detected - possible fragmentation" << std::endl;
        }
        std::cout << std::endl;
    }
};

int main(int argc, char* argv[]) {
    try {
        std::cout << "SlickLogger Memory Benchmark" << std::endl;
        std::cout << "===========================" << std::endl << std::endl;
        
        FileUtils::cleanup_test_files();
        FileUtils::create_test_directory();
        
        MemoryAnalyzer analyzer;
        analyzer.run_memory_analysis();
        
        std::cout << "Memory benchmark completed." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Memory benchmark failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}