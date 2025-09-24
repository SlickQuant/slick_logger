#include <slick_logger/logger.hpp>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>
#include <algorithm>
#include <mutex>

// Custom sink example: JSON structured logging
class JsonSink : public slick_logger::ISink {
private:
    std::ofstream file_;
    bool first_entry_ = true;
    
public:
    explicit JsonSink(const std::filesystem::path& filename) 
        : file_(filename) {
        if (!file_) {
            throw std::runtime_error("Failed to open JSON log file: " + filename.string());
        }
        file_ << "[\n"; // Start JSON array
    }
    
    ~JsonSink() {
        if (file_.is_open()) {
            file_ << "\n]\n"; // Close JSON array
        }
    }
    
    void write(const slick_logger::LogEntry& entry) override {
        // Convert log level to string
        const char* level_str = to_string(entry.level);
        // Format timestamp
        time_t time_val = static_cast<time_t>(entry.timestamp / 1000000000);
        std::tm tm = *std::localtime(&time_val);
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &tm);
        
        // Get formatted message
        auto [message, good] = format_log_message(entry);
        if (!good) {
            level_str = "ERROR";
        }
        
        // Escape JSON strings (basic implementation)
        std::string escaped_message = message;
        std::replace(escaped_message.begin(), escaped_message.end(), '"', '\'');
        std::replace(escaped_message.begin(), escaped_message.end(), '\n', ' ');
        
        // Write JSON entry
        if (!first_entry_) {
            file_ << ",\n";
        }
        first_entry_ = false;
        
        file_ << "  {\n"
              << "    \"timestamp\": \"" << timestamp << "\",\n"
              << "    \"level\": \"" << level_str << "\",\n"
              << "    \"message\": \"" << escaped_message << "\",\n"
              << "    \"thread_id\": \"" << std::this_thread::get_id() << "\"\n"
              << "  }";
    }
    
    void flush() override {
        file_.flush();
    }
};

// Custom sink example: Memory buffer for testing
class MemorySink : public slick_logger::ISink {
private:
    std::vector<std::string> entries_;
    mutable std::mutex mutex_;
    
public:
    void write(const slick_logger::LogEntry& entry) override {
        // Format the log entry
        time_t time_val = static_cast<time_t>(entry.timestamp / 1000000000);
        std::tm tm = *std::localtime(&time_val);
        
        const char* level_str = to_string(entry.level);
        char time_str[20];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);
        
        auto [message, good] = format_log_message(entry);
        if (!good) {
            level_str = "ERROR";
        }
        std::string formatted = std::string(time_str) + " [" + level_str + "] " + message;
        
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back(formatted);
    }
    
    void flush() override {
        // Nothing to flush for memory sink
    }
    
    std::vector<std::string> get_entries() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }
};

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
    Logger::instance().set_level(LogLevel::L_DEBUG);
    
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
    
    // Example 6: Custom Sinks
    std::cout << "\n=== Example 6: Custom Sinks ===\n";
    Logger::instance().clear_sinks();
    
    // Create custom sinks
    auto json_sink = std::make_shared<JsonSink>("structured.json");
    auto memory_sink = std::make_shared<MemorySink>();
    
    Logger::instance().add_sink(json_sink);
    Logger::instance().add_sink(memory_sink);
    Logger::instance().add_console_sink(false, false); // Also to console for visibility
    Logger::instance().init(1024);
    
    // Generate some log entries
    LOG_INFO("Starting custom sink demonstration");
    LOG_WARN("Custom sinks can format data however you need");
    LOG_ERROR("JSON sink creates structured logs");
    LOG_DEBUG("Memory sink stores entries in RAM for testing");
    
    // Multi-threaded logging to test thread safety
    std::thread custom_thread([]() {
        for (int i = 0; i < 5; ++i) {
            LOG_INFO("Custom thread message #{}", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    custom_thread.join();
    
    Logger::instance().shutdown();
    
    // Display memory sink contents
    std::cout << "\nMemory sink captured " << memory_sink->size() << " entries:\n";
    for (const auto& entry : memory_sink->get_entries()) {
        std::cout << "  " << entry << "\n";
    }
    
    std::cout << "\nJSON structured logs written to 'structured.json'\n";
    
    // Example 7: Complex multi-sink configuration
    std::cout << "\n=== Example 7: Complex Multi-Sink Setup ===\n";
    
    LogConfig complex_config;
    complex_config.sinks.push_back(std::make_shared<ConsoleSink>(true, true));
    complex_config.sinks.push_back(std::make_shared<FileSink>("application.log"));
    
    RotationConfig error_rotation;
    error_rotation.max_file_size = 5 * 1024 * 1024; // 5MB
    error_rotation.max_files = 10;
    complex_config.sinks.push_back(std::make_shared<RotatingFileSink>("errors.log", error_rotation));
    
    complex_config.min_level = LogLevel::L_TRACE;
    complex_config.log_queue_size = 8192;
    
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
    std::cout << "- structured.json (custom JSON sink)\n";
    std::cout << "- application.log\n";
    std::cout << "- errors.log\n";
    
    return 0;
}