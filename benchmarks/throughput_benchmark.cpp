#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <map>

#include <slick/logger.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>

#include "benchmark_utils.hpp"
#include "system_monitor.hpp"

using namespace benchmark_utils;

// Comprehensive throughput testing with scaling analysis
class ThroughputTester {
public:
    struct ThroughputResult {
        std::string logger_name;
        size_t num_threads;
        double throughput_ops_sec;
        double cpu_percent;
        size_t memory_mb;
        double latency_p99_us;
    };

    void run_scaling_test() {
        std::cout << "=== THROUGHPUT SCALING ANALYSIS ===" << std::endl;
        
        std::vector<size_t> thread_counts = {1, 2, 4, 8, 16};
        std::vector<ThroughputResult> results;
        
        for (size_t threads : thread_counts) {
            std::cout << "\nTesting with " << threads << " threads:" << std::endl;
            
            // Test SlickLogger
            results.push_back(test_slick_logger_throughput(threads));
            
            // Test spdlog async
            results.push_back(test_spdlog_async_throughput(threads));
            
            // Test spdlog sync (only up to 4 threads to avoid excessive slowness)
            if (threads <= 4) {
                results.push_back(test_spdlog_sync_throughput(threads));
            }
        }
        
        print_scaling_analysis(results);
    }

private:
    ThroughputResult test_slick_logger_throughput(size_t num_threads) {
        slick::logger::Logger::instance().reset();
        slick::logger::Logger::instance().add_file_sink(
            FileUtils::get_unique_filename("slick_throughput"));
        slick::logger::Logger::instance().init(65536);

        SystemMonitor monitor;
        monitor.start_monitoring();
        
        const size_t messages_per_thread = 100000;
        const size_t total_messages = messages_per_thread * num_threads;
        
        std::vector<std::thread> threads;
        std::atomic<size_t> counter{0};
        ThreadBarrier barrier(num_threads);
        MessageGenerator msg_gen;
        
        // Start timing
        Timer timer;
        
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([&barrier, &counter, &msg_gen, messages_per_thread]() {
                MessageGenerator local_gen; // Thread-local generator
                barrier.wait(); // Synchronized start
                
                for (size_t j = 0; j < messages_per_thread; ++j) {
                    LOG_INFO("Thread message {} - {}", counter.fetch_add(1), 
                           local_gen.generate_small());
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        double elapsed_ms = timer.elapsed_ms();
        double throughput = (total_messages / elapsed_ms) * 1000.0;
        
        monitor.stop_monitoring();
        auto usage = monitor.get_current_usage();
        
        slick::logger::Logger::instance().shutdown();
        
        std::cout << "SlickLogger (" << num_threads << " threads): " 
                  << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
        
        return {"SlickLogger", num_threads, throughput, usage.cpu_percent, 
                usage.memory_peak_bytes / 1024 / 1024, 0.0};
    }

    ThroughputResult test_spdlog_async_throughput(size_t num_threads) {
        spdlog::shutdown(); // Clean slate
        spdlog::init_thread_pool(65536, 1);
        
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            FileUtils::get_unique_filename("spdlog_async_throughput"));
        auto logger = std::make_shared<spdlog::async_logger>("spdlog_async", file_sink, 
                                                            spdlog::thread_pool());
        logger->set_level(spdlog::level::info);

        SystemMonitor monitor;
        monitor.start_monitoring();
        
        const size_t messages_per_thread = 100000;
        const size_t total_messages = messages_per_thread * num_threads;
        
        std::vector<std::thread> threads;
        std::atomic<size_t> counter{0};
        ThreadBarrier barrier(num_threads);
        
        Timer timer;
        
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([&barrier, &counter, logger, messages_per_thread]() {
                MessageGenerator local_gen;
                barrier.wait();
                
                for (size_t j = 0; j < messages_per_thread; ++j) {
                    logger->info("Thread message {} - {}", counter.fetch_add(1), 
                               local_gen.generate_small());
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        double elapsed_ms = timer.elapsed_ms();
        double throughput = (total_messages / elapsed_ms) * 1000.0;
        
        monitor.stop_monitoring();
        auto usage = monitor.get_current_usage();
        
        logger->flush();
        spdlog::shutdown();
        
        std::cout << "spdlog async (" << num_threads << " threads): " 
                  << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
        
        return {"spdlog_async", num_threads, throughput, usage.cpu_percent, 
                usage.memory_peak_bytes / 1024 / 1024, 0.0};
    }

    ThroughputResult test_spdlog_sync_throughput(size_t num_threads) {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            FileUtils::get_unique_filename("spdlog_sync_throughput"));
        auto logger = std::make_shared<spdlog::logger>("spdlog_sync", file_sink);
        logger->set_level(spdlog::level::info);

        SystemMonitor monitor;
        monitor.start_monitoring();
        
        const size_t messages_per_thread = 50000; // Fewer messages for sync to avoid excessive test time
        const size_t total_messages = messages_per_thread * num_threads;
        
        std::vector<std::thread> threads;
        std::atomic<size_t> counter{0};
        ThreadBarrier barrier(num_threads);
        
        Timer timer;
        
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([&barrier, &counter, logger, messages_per_thread]() {
                MessageGenerator local_gen;
                barrier.wait();
                
                for (size_t j = 0; j < messages_per_thread; ++j) {
                    logger->info("Thread message {} - {}", counter.fetch_add(1), 
                               local_gen.generate_small());
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        double elapsed_ms = timer.elapsed_ms();
        double throughput = (total_messages / elapsed_ms) * 1000.0;
        
        monitor.stop_monitoring();
        auto usage = monitor.get_current_usage();
        
        logger->flush();
        
        std::cout << "spdlog sync  (" << num_threads << " threads): " 
                  << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
        
        return {"spdlog_sync", num_threads, throughput, usage.cpu_percent, 
                usage.memory_peak_bytes / 1024 / 1024, 0.0};
    }

    void print_scaling_analysis(const std::vector<ThroughputResult>& results) {
        std::cout << "\n=== SCALING ANALYSIS ===" << std::endl;
        std::cout << std::left << std::setw(15) << "Logger" 
                  << std::right << std::setw(8) << "Threads"
                  << std::right << std::setw(12) << "Throughput"
                  << std::right << std::setw(10) << "CPU %"
                  << std::right << std::setw(10) << "Memory MB"
                  << std::right << std::setw(12) << "Efficiency" << std::endl;
        std::cout << std::string(75, '-') << std::endl;

        // Group results by logger
        std::map<std::string, std::vector<ThroughputResult>> by_logger;
        for (const auto& result : results) {
            by_logger[result.logger_name].push_back(result);
        }

        for (auto& [logger_name, logger_results] : by_logger) {
            std::sort(logger_results.begin(), logger_results.end(),
                     [](const auto& a, const auto& b) { return a.num_threads < b.num_threads; });

            double baseline_throughput = 0.0;
            for (const auto& result : logger_results) {
                if (result.num_threads == 1) {
                    baseline_throughput = result.throughput_ops_sec;
                    break;
                }
            }

            for (const auto& result : logger_results) {
                double efficiency = baseline_throughput > 0 ? 
                    (result.throughput_ops_sec / baseline_throughput) / result.num_threads * 100.0 : 0.0;

                std::cout << std::left << std::setw(15) << result.logger_name
                          << std::right << std::setw(8) << result.num_threads
                          << std::right << std::setw(12) << std::fixed << std::setprecision(0) 
                          << result.throughput_ops_sec
                          << std::right << std::setw(10) << std::fixed << std::setprecision(1) 
                          << result.cpu_percent
                          << std::right << std::setw(10) << result.memory_mb
                          << std::right << std::setw(11) << std::fixed << std::setprecision(1) 
                          << efficiency << "%" << std::endl;
            }
            std::cout << std::endl;
        }
    }
};

// Burst throughput test - how well loggers handle sudden spikes
void test_burst_performance() {
    std::cout << "=== BURST PERFORMANCE TEST ===" << std::endl;
    
    const size_t burst_size = 50000;
    const size_t num_bursts = 5;
    const auto burst_interval = std::chrono::milliseconds(1000);
    
    // Test SlickLogger burst performance
    {
        std::cout << "Testing SlickLogger burst handling..." << std::endl;
        
        slick::logger::Logger::instance().reset();
        slick::logger::Logger::instance().add_file_sink(
            FileUtils::get_unique_filename("slick_burst"));
        slick::logger::Logger::instance().init(65536);

        SystemMonitor monitor;
        monitor.start_monitoring();
        
        MessageGenerator msg_gen;
        std::vector<double> burst_throughputs;
        
        for (size_t burst = 0; burst < num_bursts; ++burst) {
            Timer burst_timer;
            
            for (size_t i = 0; i < burst_size; ++i) {
                LOG_INFO("Burst {} message {} - {}", burst, i, msg_gen.generate_small());
            }
            
            double burst_time_ms = burst_timer.elapsed_ms();
            double burst_throughput = (burst_size / burst_time_ms) * 1000.0;
            burst_throughputs.push_back(burst_throughput);
            
            std::cout << "  Burst " << burst << ": " << std::fixed << std::setprecision(0) 
                      << burst_throughput << " ops/sec" << std::endl;
            
            if (burst < num_bursts - 1) {
                std::this_thread::sleep_for(burst_interval);
            }
        }
        
        monitor.stop_monitoring();
        auto usage = monitor.get_current_usage();
        
        Statistics burst_stats(burst_throughputs);
        std::cout << "SlickLogger burst stats:" << std::endl;
        std::cout << "  Mean: " << burst_stats.mean() << " ops/sec" << std::endl;
        std::cout << "  StdDev: " << burst_stats.std_dev() << " ops/sec" << std::endl;
        usage.print();
        
        slick::logger::Logger::instance().shutdown();
    }
    
    // Compare with spdlog async
    {
        std::cout << "Testing spdlog async burst handling..." << std::endl;
        
        spdlog::init_thread_pool(65536, 1);
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            FileUtils::get_unique_filename("spdlog_async_burst"));
        auto logger = std::make_shared<spdlog::async_logger>("spdlog_async", file_sink, 
                                                            spdlog::thread_pool());
        logger->set_level(spdlog::level::info);

        SystemMonitor monitor;
        monitor.start_monitoring();
        
        MessageGenerator msg_gen;
        std::vector<double> burst_throughputs;
        
        for (size_t burst = 0; burst < num_bursts; ++burst) {
            Timer burst_timer;
            
            for (size_t i = 0; i < burst_size; ++i) {
                logger->info("Burst {} message {} - {}", burst, i, msg_gen.generate_small());
            }
            
            double burst_time_ms = burst_timer.elapsed_ms();
            double burst_throughput = (burst_size / burst_time_ms) * 1000.0;
            burst_throughputs.push_back(burst_throughput);
            
            std::cout << "  Burst " << burst << ": " << std::fixed << std::setprecision(0) 
                      << burst_throughput << " ops/sec" << std::endl;
            
            if (burst < num_bursts - 1) {
                std::this_thread::sleep_for(burst_interval);
            }
        }
        
        monitor.stop_monitoring();
        auto usage = monitor.get_current_usage();
        
        Statistics burst_stats(burst_throughputs);
        std::cout << "spdlog async burst stats:" << std::endl;
        std::cout << "  Mean: " << burst_stats.mean() << " ops/sec" << std::endl;
        std::cout << "  StdDev: " << burst_stats.std_dev() << " ops/sec" << std::endl;
        usage.print();
        
        logger->flush();
        spdlog::shutdown();
    }
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "SlickLogger Throughput Benchmark" << std::endl;
        std::cout << "================================" << std::endl << std::endl;
        
        FileUtils::cleanup_test_files();
        FileUtils::create_test_directory();
        
        ThroughputTester tester;
        tester.run_scaling_test();
        
        test_burst_performance();
        
        std::cout << "Throughput benchmark completed." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Throughput benchmark failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}