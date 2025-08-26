#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

// Prevent Windows min/max macros
#define NOMINMAX
#ifdef min
#undef min
#endif
#ifdef max  
#undef max
#endif

#include <slick_logger/logger.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <fmt/format.h>

using namespace std::chrono;

class QuickTimer {
public:
    void start() { start_ = high_resolution_clock::now(); }
    double stop() {
        auto end = high_resolution_clock::now();
        return duration_cast<microseconds>(end - start_).count() / 1000.0;
    }
    double rate(size_t count, double elapsed_ms) {
        return (count * 1000.0) / elapsed_ms;
    }

private:
    high_resolution_clock::time_point start_;
};

int main() {
    std::cout << "Quick Benchmark: SlickLogger vs spdlog\n";
    std::cout << "=====================================\n";
    
    const size_t test_messages = 10000; // Reduced for quick test
    QuickTimer timer;
    
    // SlickLogger test
    std::cout << "\nSlickLogger (single thread):\n";
    slick_logger::Logger::instance().reset();
    slick_logger::Logger::instance().add_file_sink("bench_slick.log");
    slick_logger::Logger::instance().init(8192);
    
    timer.start();
    for (size_t i = 0; i < test_messages; ++i) {
        LOG_INFO("Benchmark message {} value: {:.3f}", i, i * 1.618);
    }
    double slick_time = timer.stop();
    slick_logger::Logger::instance().shutdown();
    
    std::cout << "Time: " << slick_time << " ms\n";
    std::cout << "Rate: " << static_cast<size_t>(timer.rate(test_messages, slick_time)) << " msg/sec\n";
    
    // spdlog test
    std::cout << "\nspdlog (single thread):\n";
    auto logger = spdlog::basic_logger_mt("bench", "bench_spdlog.log");
    
    timer.start();
    for (size_t i = 0; i < test_messages; ++i) {
        logger->info("Benchmark message {} value: {:.3f}", i, i * 1.618);
    }
    double spdlog_time = timer.stop();
    spdlog::drop("bench");
    
    std::cout << "Time: " << spdlog_time << " ms\n";
    std::cout << "Rate: " << static_cast<size_t>(timer.rate(test_messages, spdlog_time)) << " msg/sec\n";
    
    // Multi-threaded SlickLogger
    std::cout << "\nSlickLogger (4 threads):\n";
    slick_logger::Logger::instance().reset();
    slick_logger::Logger::instance().add_file_sink("bench_slick_mt.log");
    slick_logger::Logger::instance().init(8192);
    
    const size_t num_threads = 4;
    const size_t msgs_per_thread = test_messages / num_threads;
    
    timer.start();
    std::vector<std::thread> threads;
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, msgs_per_thread]() {
            for (size_t i = 0; i < msgs_per_thread; ++i) {
                LOG_INFO("Thread {} msg {} val: {:.2f}", t, i, i * 2.718);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    double slick_mt_time = timer.stop();
    slick_logger::Logger::instance().shutdown();
    
    std::cout << "Time: " << slick_mt_time << " ms\n";
    std::cout << "Rate: " << static_cast<size_t>(timer.rate(test_messages, slick_mt_time)) << " msg/sec\n";
    
    // Multi-threaded spdlog
    std::cout << "\nspdlog (4 threads):\n";
    auto mt_logger = spdlog::basic_logger_mt("bench_mt", "bench_spdlog_mt.log");
    
    timer.start();
    threads.clear();
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([mt_logger, t, msgs_per_thread]() {
            for (size_t i = 0; i < msgs_per_thread; ++i) {
                mt_logger->info("Thread {} msg {} val: {:.2f}", t, i, i * 2.718);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    double spdlog_mt_time = timer.stop();
    spdlog::drop("bench_mt");
    
    std::cout << "Time: " << spdlog_mt_time << " ms\n";
    std::cout << "Rate: " << static_cast<size_t>(timer.rate(test_messages, spdlog_mt_time)) << " msg/sec\n";
    
    // Summary
    std::cout << "\n=== Performance Summary ===\n";
    std::cout << fmt::format("SlickLogger single-thread: {:.1f}x faster than spdlog\n", 
                            spdlog_time / slick_time);
    std::cout << fmt::format("SlickLogger multi-thread:  {:.1f}x faster than spdlog\n", 
                            spdlog_mt_time / slick_mt_time);
    std::cout << fmt::format("SlickLogger scaling:       {:.1f}x speedup with 4 threads\n", 
                            slick_time / slick_mt_time);
    std::cout << fmt::format("spdlog scaling:            {:.1f}x speedup with 4 threads\n", 
                            spdlog_time / spdlog_mt_time);
    
    return 0;
}