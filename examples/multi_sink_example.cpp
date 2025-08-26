#include <slick_logger/logger.hpp>
#include <thread>
#include <chrono>

int main() {
    using namespace slick_logger;
    
    // Example 1: Traditional single file sink (backwards compatible)
    std::cout << "=== Example 1: Traditional File Logging ===\n";
    Logger::instance().init("traditional.log", 1024);
    LOG_INFO("This goes to traditional.log only");
    Logger::instance().shutdown();
    
    // Example 2: Console sink only
    std::cout << "\n=== Example 2: Console Logging ===\n";
    Logger::instance().clear_sinks();
    Logger::instance().add_console_sink(true, true); // colors enabled, errors to stderr
    Logger::instance().init(1024);
    
    LOG_INFO("Console message in green");
    LOG_WARN("Warning message in yellow");
    LOG_ERROR("Error message in red (goes to stderr)");
    Logger::instance().shutdown();
    
    // Example 3: Multiple sinks (console + file)
    std::cout << "\n=== Example 3: Multiple Sinks (Console + File) ===\n";
    Logger::instance().clear_sinks();
    Logger::instance().add_console_sink(true, true);
    Logger::instance().add_file_sink("multi_sink.log");
    Logger::instance().init(1024);
    Logger::instance().set_log_level(LogLevel::DEBUG);
    
    LOG_DEBUG("This appears on both console and file");
    LOG_INFO("Multi-sink logging is working!");
    LOG_WARN("Warning appears in both places");
    Logger::instance().shutdown();
    
    // Example 4: Rotating file sink
    std::cout << "\n=== Example 4: Rotating File Sink ===\n";
    Logger::instance().clear_sinks();
    Logger::instance().add_console_sink(false, true); // no colors for clarity
    
    RotationConfig rotation_config;
    rotation_config.max_file_size = 1024; // Very small for demo (1KB)
    rotation_config.max_files = 3;
    Logger::instance().add_rotating_file_sink("rotating.log", rotation_config);
    Logger::instance().init(1024);
    
    // Generate enough logs to trigger rotation
    for (int i = 0; i < 50; ++i) {
        LOG_INFO("Rotation test message #{} - this should trigger file rotation when size limit is reached", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Logger::instance().shutdown();
    
    // Example 5: Daily file sink
    std::cout << "\n=== Example 5: Daily File Sink ===\n";
    Logger::instance().clear_sinks();
    Logger::instance().add_console_sink(false, true);
    
    RotationConfig daily_config;
    Logger::instance().add_daily_file_sink("daily.log", daily_config);
    Logger::instance().init(1024);
    
    LOG_INFO("Daily log entry - filename includes today's date");
    Logger::instance().shutdown();
    
    // Example 6: Complex multi-sink configuration
    std::cout << "\n=== Example 6: Complex Multi-Sink Setup ===\n";
    
    LogConfig complex_config;
    complex_config.sinks.push_back(std::make_shared<ConsoleSink>(true, true));
    complex_config.sinks.push_back(std::make_shared<FileSink>("application.log"));
    
    RotationConfig error_rotation;
    error_rotation.max_file_size = 5 * 1024 * 1024; // 5MB
    error_rotation.max_files = 10;
    complex_config.sinks.push_back(std::make_shared<RotatingFileSink>("errors.log", error_rotation));
    
    complex_config.min_level = LogLevel::TRACE;
    complex_config.queue_size = 8192;
    
    Logger::instance().init(complex_config);
    
    // Multi-threaded logging test
    std::thread t1([]() {
        for (int i = 0; i < 10; ++i) {
            LOG_INFO("Thread 1: Processing item {}", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    std::thread t2([]() {
        for (int i = 0; i < 10; ++i) {
            LOG_WARN("Thread 2: Warning #{}", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    std::thread t3([]() {
        for (int i = 0; i < 5; ++i) {
            LOG_ERROR("Thread 3: Simulated error #{}", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    t1.join();
    t2.join();
    t3.join();
    
    LOG_FATAL("Application shutting down");
    Logger::instance().shutdown();
    
    std::cout << "\n=== All examples completed ===\n";
    std::cout << "Check the generated log files:\n";
    std::cout << "- traditional.log\n";
    std::cout << "- multi_sink.log\n";
    std::cout << "- rotating.log (and rotating_1.log, rotating_2.log if rotation occurred)\n";
    std::cout << "- daily_YYYY-MM-DD.log\n";
    std::cout << "- application.log\n";
    std::cout << "- errors.log\n";
    
    return 0;
}