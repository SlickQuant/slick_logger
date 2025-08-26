#include <iostream>
#include <slick_logger/logger.hpp>
#include <thread>
#include <chrono>

using namespace slick_logger;

int main() {
    std::cout << "SlickLogger Timestamp Format Example\n";
    std::cout << "====================================\n\n";

    // Test different timestamp formats
    
    // 1. Default microsecond precision
    Logger::instance().reset();
    Logger::instance().add_console_sink();  // Uses WITH_MICROSECONDS by default
    Logger::instance().init(8192);
    
    std::cout << "1. Default format (WITH_MICROSECONDS):\n";
    LOG_INFO("This message shows microsecond precision");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    LOG_WARN("Warning message with microseconds");
    
    Logger::instance().shutdown();
    std::cout << "\n";
    
    // 2. Millisecond precision
    Logger::instance().reset();
    Logger::instance().add_console_sink(TimestampFormatter::Format::WITH_MILLISECONDS);
    Logger::instance().init(8192);
    
    std::cout << "2. Millisecond precision format:\n";
    LOG_INFO("This message shows millisecond precision");
    LOG_ERROR("Error message with milliseconds");
    
    Logger::instance().shutdown();
    std::cout << "\n";
    
    // 3. Traditional format (no sub-second precision)
    Logger::instance().reset();
    Logger::instance().add_console_sink(TimestampFormatter::Format::DEFAULT);
    Logger::instance().init(8192);
    
    std::cout << "3. Traditional format (no sub-second precision):\n";
    LOG_INFO("This message uses traditional timestamp format");
    LOG_DEBUG("Debug message without sub-second precision");
    
    Logger::instance().shutdown();
    std::cout << "\n";
    
    // 4. ISO8601 format
    Logger::instance().reset();
    Logger::instance().add_console_sink(TimestampFormatter::Format::ISO8601);
    Logger::instance().init(8192);
    
    std::cout << "4. ISO8601 format:\n";
    LOG_INFO("This message uses ISO8601 timestamp format");
    LOG_TRACE("Trace message in ISO8601 format");
    
    Logger::instance().shutdown();
    std::cout << "\n";
    
    // 5. Time only format
    Logger::instance().reset();
    Logger::instance().add_console_sink(TimestampFormatter::Format::TIME_ONLY);
    Logger::instance().init(8192);
    
    std::cout << "5. Time only format:\n";
    LOG_INFO("This message shows only time (no date)");
    LOG_FATAL("Fatal message with time only");
    
    Logger::instance().shutdown();
    std::cout << "\n";
    
    // 6. Custom format example
    Logger::instance().reset();
    Logger::instance().add_console_sink(std::string("[%H:%M:%S] "));  // Custom format: just time in brackets
    Logger::instance().init(8192);
    
    std::cout << "6. Custom format ([HH:MM:SS]):\n";
    LOG_INFO("This message uses a custom timestamp format");
    LOG_WARN("Custom format warning message");
    
    Logger::instance().shutdown();
    std::cout << "\n";
    
    // 7. Demonstrate file logging with different formats
    std::cout << "7. File logging with different timestamp formats:\n";
    std::cout << "Creating log files with different timestamp formats...\n";
    
    Logger::instance().reset();
    Logger::instance().add_file_sink("default_timestamps.log");  // Default microseconds
    Logger::instance().add_file_sink("millisecond_timestamps.log", TimestampFormatter::Format::WITH_MILLISECONDS);
    Logger::instance().add_file_sink("iso8601_timestamps.log", TimestampFormatter::Format::ISO8601);
    Logger::instance().add_file_sink("custom_timestamps.log", "%Y%m%d_%H%M%S");  // Custom: YYYYMMDD_HHMMSS
    Logger::instance().init(8192);
    
    LOG_INFO("This message will appear in all log files with different timestamp formats");
    LOG_WARN("Warning logged to multiple files with different timestamps");
    LOG_ERROR("Error message demonstrating timestamp variety");
    
    Logger::instance().shutdown();
    
    std::cout << "Log files created:\n";
    std::cout << "  - default_timestamps.log (microseconds)\n";
    std::cout << "  - millisecond_timestamps.log (milliseconds)\n";
    std::cout << "  - iso8601_timestamps.log (ISO8601 format)\n";
    std::cout << "  - custom_timestamps.log (YYYYMMDD_HHMMSS format)\n\n";
    
    std::cout << "Example complete! Check the log files to see different timestamp formats.\n";
    
    return 0;
}