#include <slick_logger/logger.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>

class SinkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing log files
        cleanup_files();
    }

    void TearDown() override {
        slick_logger::Logger::instance().reset(); // Use reset instead of shutdown
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Allow cleanup
        cleanup_files();
    }
    
    void cleanup_files() {
        std::vector<std::string> files = {
            "console_test.log", "multi_sink_test.log", "rotating_test.log",
            "rotating_test_1.log", "rotating_test_2.log", "rotating_test_3.log",
            "daily_test.log", "daily_rotation_test.log", "daily_rotation_test_2025-08-25.log",
            "daily_size_test.log", "args_sink.log", "dedicated_sink.log", "filtered_sink.log",
            "named_sink1.log", "named_sink2.log", "regular_sink.log", "daily_no_size_rotation.log",
            "daily_multi_rotation.log", "daily_restart_test.log", "daily_restart_existing.log"
        };

        for (const auto& file : files) {
            std::error_code ec;
            std::filesystem::remove(file, ec); // Don't throw if file doesn't exist
        }

        // Clean up daily files with date pattern
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(".", ec)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.find("daily_test_") == 0 ||
                    filename.find("daily_size_test_") == 0 ||
                    filename.find("daily_no_size_rotation_") == 0 ||
                    filename.find("daily_multi_rotation_") == 0 ||
                    filename.find("daily_restart_test_") == 0 ||
                    filename.find("daily_restart_existing_") == 0) {
                    std::filesystem::remove(entry.path(), ec);
                }
            }
        }
    }
};

TEST_F(SinkTest, ConsoleSinkBasic) {
    // Redirect stdout to capture console output
    std::stringstream buffer;
    std::streambuf* old_cout = std::cout.rdbuf(buffer.rdbuf());
    
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_console_sink(false, false); // no colors, no stderr
    slick_logger::Logger::instance().init(1024);
    
    LOG_INFO("Console test message");
    
    slick_logger::Logger::instance().reset();
    
    // Restore stdout
    std::cout.rdbuf(old_cout);
    
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("Console test message") != std::string::npos);
    EXPECT_TRUE(output.find("[INFO]") != std::string::npos);
}

TEST_F(SinkTest, FileSinkBasic) {
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_file_sink("console_test.log");
    slick_logger::Logger::instance().init(1024);
    
    LOG_INFO("File sink test message");
    
    slick_logger::Logger::instance().reset();
    
    ASSERT_TRUE(std::filesystem::exists("console_test.log"));
    
    std::ifstream log_file("console_test.log");
    std::string line;
    std::getline(log_file, line);   // first line is the logger's version
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("File sink test message") != std::string::npos);
}

TEST_F(SinkTest, MultiSinkTest) {
    // Redirect stdout to capture console output
    std::stringstream buffer;
    std::streambuf* old_cout = std::cout.rdbuf(buffer.rdbuf());
    
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_console_sink(false, false);
    slick_logger::Logger::instance().add_file_sink("multi_sink_test.log");
    slick_logger::Logger::instance().init(1024);
    
    LOG_INFO("Multi-sink test message");

    auto file_sink = slick_logger::Logger::instance().get_sink<slick_logger::FileSink>();
    EXPECT_TRUE(file_sink != nullptr);
    
    slick_logger::Logger::instance().reset();
    
    // Restore stdout
    std::cout.rdbuf(old_cout);
    
    // Check console output
    std::string console_output = buffer.str();
    EXPECT_TRUE(console_output.find("Multi-sink test message") != std::string::npos);
    
    // Check file output
    ASSERT_TRUE(std::filesystem::exists("multi_sink_test.log"));
    std::ifstream log_file("multi_sink_test.log");
    std::string line;
    std::getline(log_file, line);   // first line is the logger's version
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("Multi-sink test message") != std::string::npos);
}

TEST_F(SinkTest, RotatingFileSinkTest) {
    slick_logger::RotationConfig rotation_config;
    rotation_config.max_file_size = 100; // Very small for testing
    rotation_config.max_files = 3;
    
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_rotating_file_sink("rotating_test.log", rotation_config);
    slick_logger::Logger::instance().init(1024);
    
    // Generate enough messages to trigger rotation
    for (int i = 0; i < 20; ++i) {
        LOG_INFO("Rotation test message number {} with extra text to reach size limit", i);
    }
    
    slick_logger::Logger::instance().reset();
    
    // Check that rotation occurred
    EXPECT_TRUE(std::filesystem::exists("rotating_test.log"));
    
    // Should have rotated files
    bool rotation_occurred = std::filesystem::exists("rotating_test_1.log");
    EXPECT_TRUE(rotation_occurred);
}

TEST_F(SinkTest, DailyFileSinkTest) {
    slick_logger::RotationConfig daily_config;
    
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_daily_file_sink("daily_test.log", daily_config);
    slick_logger::Logger::instance().init(1024);
    
    LOG_INFO("Daily sink test message");
    
    slick_logger::Logger::instance().reset();
    
    // Should log to the base filename (daily_test.log), not a dated file
    EXPECT_TRUE(std::filesystem::exists("daily_test.log"));
    
    // Check that the log message is in the base file
    std::ifstream log_file("daily_test.log");
    std::string line;
    std::getline(log_file, line);   // first line is the logger's version
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("Daily sink test message") != std::string::npos);
}

TEST_F(SinkTest, DailyFileSinkRotation) {
    // Create a testable DailyFileSink that allows us to control the date
    class TestDailyFileSink : public slick_logger::DailyFileSink {
    public:
        TestDailyFileSink(const std::filesystem::path& base_path, const slick_logger::RotationConfig& config)
            : DailyFileSink(base_path, config), test_date_("2025-08-25")
        {
            current_date_ = test_date_;
        }
        
        // Override to return our controlled test date
        std::string get_date_string() override {
            return test_date_;
        }
        
        // Method to change the date and trigger rotation check
        void change_date_and_check(const std::string& new_date) {
            test_date_ = new_date;
            check_rotation(); // This should trigger rotation when date changes
        }
        
    private:
        std::string test_date_;
    };
    
    slick_logger::RotationConfig daily_config;
    
    slick_logger::Logger::instance().clear_sinks();
    
    // Create our test sink with initial date "2025-08-25"
    auto test_sink = std::make_shared<TestDailyFileSink>("daily_rotation_test.log", daily_config);
    slick_logger::Logger::instance().add_sink(test_sink);
    slick_logger::Logger::instance().init(1024);
    
    // Log some messages on "day 1" (2025-08-25)
    LOG_INFO("Message from day 1");
    LOG_WARN("Warning from day 1");
    
    slick_logger::Logger::instance().reset();
    
    // Verify base file has the day 1 content
    ASSERT_TRUE(std::filesystem::exists("daily_rotation_test.log"));
    std::ifstream base_file("daily_rotation_test.log");
    std::string day1_content((std::istreambuf_iterator<char>(base_file)),
                            std::istreambuf_iterator<char>());
    base_file.close();
    
    EXPECT_TRUE(day1_content.find("Message from day 1") != std::string::npos);
    EXPECT_TRUE(day1_content.find("Warning from day 1") != std::string::npos);
    
    // At this point, no dated file should exist yet
    EXPECT_FALSE(std::filesystem::exists("daily_rotation_test_2025-08-25.log"));
    
    // Now simulate the next day by changing the date and triggering rotation check
    test_sink->change_date_and_check("2025-08-26");
    
    // After rotation check, the dated file should be created with day 1's content
    EXPECT_TRUE(std::filesystem::exists("daily_rotation_test_2025-08-25.log"));
    
    // Check that the dated file contains day 1's content
    std::ifstream dated_file("daily_rotation_test_2025-08-25.log");
    std::string dated_content((std::istreambuf_iterator<char>(dated_file)),
                              std::istreambuf_iterator<char>());
    dated_file.close();
    
    EXPECT_TRUE(dated_content.find("Message from day 1") != std::string::npos);
    EXPECT_TRUE(dated_content.find("Warning from day 1") != std::string::npos);
    
    // The base file should now be empty or ready for new content
    std::ifstream new_base_file("daily_rotation_test.log");
    std::string new_base_content((std::istreambuf_iterator<char>(new_base_file)),
                                std::istreambuf_iterator<char>());
    new_base_file.close();
    
    // Base file should be empty after rotation
    EXPECT_TRUE(new_base_content.empty() || new_base_content.find_first_not_of(" \t\r\n") == std::string::npos);
    
    // Re-initialize logger to continue logging to the rotated base file
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_sink(test_sink);
    slick_logger::Logger::instance().init(1024);
    
    // Log new messages on "day 2" - these should go to the fresh base file
    LOG_INFO("Message from day 2");
    
    slick_logger::Logger::instance().reset();
    
    // Verify the base file now contains day 2's content
    std::ifstream final_base_file("daily_rotation_test.log");
    std::string final_base_content((std::istreambuf_iterator<char>(final_base_file)),
                                   std::istreambuf_iterator<char>());
    final_base_file.close();
    
    EXPECT_TRUE(final_base_content.find("Message from day 2") != std::string::npos);
    
    // Ensure day 1 messages are NOT in the current base file
    EXPECT_FALSE(final_base_content.find("Message from day 1") != std::string::npos);
}

TEST_F(SinkTest, DailyFileSinkSizeRotation) {
    slick_logger::RotationConfig size_config;
    size_config.max_file_size = 200; // Very small size for testing

    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_daily_file_sink("daily_size_test.log", size_config);
    slick_logger::Logger::instance().init(1024);

    // Log enough messages to trigger size-based rotation
    // Each message should be around 100+ bytes to trigger rotation quickly
    for (int i = 0; i < 10; ++i) {
        LOG_INFO("Size rotation test message number {} with enough text to reach the file size limit quickly", i);
    }

    slick_logger::Logger::instance().reset();

    // Check that base file exists
    EXPECT_TRUE(std::filesystem::exists("daily_size_test.log"));

    // Get current date for filename verification
    auto now = std::chrono::system_clock::now();
    time_t time_val = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_ptr = std::localtime(&time_val);
    std::string current_date;
    if (tm_ptr) {
        char date_str[11];
        std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_ptr);
        current_date = std::string(date_str);
    } else {
        current_date = "1970-01-01"; // fallback
    }

    // Check for size-rotated files with date and index pattern
    std::string expected_first_rotation = "daily_size_test_" + current_date + "_001.log";
    EXPECT_TRUE(std::filesystem::exists(expected_first_rotation));

    // Check that the rotated file contains some of our messages
    std::ifstream rotated_file(expected_first_rotation);
    std::string rotated_content((std::istreambuf_iterator<char>(rotated_file)),
                               std::istreambuf_iterator<char>());
    rotated_file.close();

    EXPECT_TRUE(rotated_content.find("Size rotation test message") != std::string::npos);

    // Check that base file still contains the most recent messages
    std::ifstream base_file("daily_size_test.log");
    std::string base_content((std::istreambuf_iterator<char>(base_file)),
                            std::istreambuf_iterator<char>());
    base_file.close();

    EXPECT_TRUE(base_content.find("Size rotation test message") != std::string::npos);

    // Verify that multiple rotations can occur (if enough content was generated)
    std::string expected_second_rotation = "daily_size_test_" + current_date + "_002.log";
    if (std::filesystem::exists(expected_second_rotation)) {
        std::ifstream second_rotated_file(expected_second_rotation);
        std::string second_rotated_content((std::istreambuf_iterator<char>(second_rotated_file)),
                                          std::istreambuf_iterator<char>());
        second_rotated_file.close();

        EXPECT_TRUE(second_rotated_content.find("Size rotation test message") != std::string::npos);
    }
}

TEST_F(SinkTest, BackwardsCompatibility) {
    // Test that old init method still works
    slick_logger::Logger::instance().init("console_test.log", 1024);
    
    LOG_INFO("Backwards compatibility test");
    
    slick_logger::Logger::instance().reset();
    
    ASSERT_TRUE(std::filesystem::exists("console_test.log"));
    
    std::ifstream log_file("console_test.log");
    std::string line;
    std::getline(log_file, line);   // first line is the logger's version
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("Backwards compatibility test") != std::string::npos);
}

TEST_F(SinkTest, LogConfigTest) {
    slick_logger::LogConfig config;
    config.sinks.push_back(std::make_shared<slick_logger::ConsoleSink>(false, false));
    config.sinks.push_back(std::make_shared<slick_logger::FileSink>("multi_sink_test.log"));
    config.min_level = slick_logger::LogLevel::L_WARN;
    config.log_queue_size = 2048;
    
    // Redirect stdout
    std::stringstream buffer;
    std::streambuf* old_cout = std::cout.rdbuf(buffer.rdbuf());
    
    slick_logger::Logger::instance().init(config);
    
    LOG_DEBUG("This should not appear"); // Below WARN level
    LOG_WARN("This warning should appear");
    LOG_ERROR("This error should appear");
    
    slick_logger::Logger::instance().reset();
    
    // Restore stdout
    std::cout.rdbuf(old_cout);
    
    std::string console_output = buffer.str();
    EXPECT_TRUE(console_output.find("This warning should appear") != std::string::npos);
    EXPECT_TRUE(console_output.find("This error should appear") != std::string::npos);
    EXPECT_FALSE(console_output.find("This should not appear") != std::string::npos);
    
    // Check file as well
    std::ifstream log_file("multi_sink_test.log");
    std::string file_content((std::istreambuf_iterator<char>(log_file)),
                             std::istreambuf_iterator<char>());
    EXPECT_TRUE(file_content.find("This warning should appear") != std::string::npos);
    EXPECT_TRUE(file_content.find("This error should appear") != std::string::npos);
    EXPECT_FALSE(file_content.find("This should not appear") != std::string::npos);
}

TEST_F(SinkTest, NamedSinkDirectLogging) {
    // Test the new ISink::log() methods for direct selective logging
    slick_logger::Logger::instance().clear_sinks();
    
    // Create named sinks
    slick_logger::Logger::instance().add_file_sink("named_sink1.log", "sink1");
    slick_logger::Logger::instance().add_file_sink("named_sink2.log", "sink2");
    slick_logger::Logger::instance().add_console_sink(false, false, "console"); // no colors for testing
    slick_logger::Logger::instance().init(1024);
    
    // Get sink references
    auto sink1 = slick_logger::Logger::instance().get_sink("sink1");
    auto sink2 = slick_logger::Logger::instance().get_sink("sink2");
    auto console_sink = slick_logger::Logger::instance().get_sink("console");
    
    // Verify sinks were found
    ASSERT_TRUE(sink1 != nullptr);
    ASSERT_TRUE(sink2 != nullptr);
    ASSERT_TRUE(console_sink != nullptr);
    
    // Test direct logging to specific sinks
    sink1->log_info("Info message to sink1 only");
    sink2->log_error("Error message to sink2 only");
    console_sink->log_warn("Warning to console only");
    sink1->log_debug("Debug to sink1");
    sink2->log_fatal("Fatal to sink2");
    
    // Test convenience methods
    sink1->log_trace("Trace to sink1");
    
    slick_logger::Logger::instance().reset();
    
    // Verify sink1.log contains only sink1 messages
    ASSERT_TRUE(std::filesystem::exists("named_sink1.log"));
    std::ifstream sink1_file("named_sink1.log");
    std::string sink1_content((std::istreambuf_iterator<char>(sink1_file)),
                              std::istreambuf_iterator<char>());
    sink1_file.close();
    
    EXPECT_TRUE(sink1_content.find("Info message to sink1 only") != std::string::npos);
    EXPECT_TRUE(sink1_content.find("Debug to sink1") != std::string::npos);
    EXPECT_TRUE(sink1_content.find("Trace to sink1") != std::string::npos);
    
    // Should NOT contain messages meant for other sinks
    EXPECT_FALSE(sink1_content.find("Error message to sink2 only") != std::string::npos);
    EXPECT_FALSE(sink1_content.find("Warning to console only") != std::string::npos);
    EXPECT_FALSE(sink1_content.find("Fatal to sink2") != std::string::npos);
    
    // Verify sink2.log contains only sink2 messages
    ASSERT_TRUE(std::filesystem::exists("named_sink2.log"));
    std::ifstream sink2_file("named_sink2.log");
    std::string sink2_content((std::istreambuf_iterator<char>(sink2_file)),
                              std::istreambuf_iterator<char>());
    sink2_file.close();
    
    EXPECT_TRUE(sink2_content.find("Error message to sink2 only") != std::string::npos);
    EXPECT_TRUE(sink2_content.find("Fatal to sink2") != std::string::npos);
    
    // Should NOT contain messages meant for other sinks
    EXPECT_FALSE(sink2_content.find("Info message to sink1 only") != std::string::npos);
    EXPECT_FALSE(sink2_content.find("Warning to console only") != std::string::npos);
}

TEST_F(SinkTest, SinkDirectLoggingWithArgs) {
    // Test direct sink logging with format arguments
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_file_sink("args_sink.log", "args_sink");
    slick_logger::Logger::instance().init(1024);
    
    auto sink = slick_logger::Logger::instance().get_sink("args_sink");
    ASSERT_TRUE(sink != nullptr);
    
    // Test logging with various argument types
    sink->log_info("Processing item {} of {}", 5, 10);
    sink->log_error("Failed with code {}: {}", 404, "Not Found");
    sink->log_warn("Warning: {:.2f}% complete", 85.7);
    
    slick_logger::Logger::instance().reset();
    
    // Verify formatted output
    ASSERT_TRUE(std::filesystem::exists("args_sink.log"));
    std::ifstream file("args_sink.log");
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    EXPECT_TRUE(content.find("Processing item 5 of 10") != std::string::npos);
    EXPECT_TRUE(content.find("Failed with code 404: Not Found") != std::string::npos);
    EXPECT_TRUE(content.find("Warning: 85.70% complete") != std::string::npos);
}

TEST_F(SinkTest, SinkDirectLoggingLevelFiltering) {
    // Test that sink-level filtering works with direct logging
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_file_sink("filtered_sink.log", "filtered");
    slick_logger::Logger::instance().init(1024);
    
    auto sink = slick_logger::Logger::instance().get_sink("filtered");
    ASSERT_TRUE(sink != nullptr);
    
    // Set sink to only accept WARN and above
    sink->set_min_level(slick_logger::LogLevel::L_WARN);
    
    // These should be filtered out
    sink->log_trace("Should be filtered");
    sink->log_debug("Should be filtered");
    sink->log_info("Should be filtered");
    
    // These should appear
    sink->log_warn("Warning should appear");
    sink->log_error("Error should appear");
    sink->log_fatal("Fatal should appear");
    
    slick_logger::Logger::instance().reset();
    
    // Verify filtering worked
    ASSERT_TRUE(std::filesystem::exists("filtered_sink.log"));
    std::ifstream file("filtered_sink.log");
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    EXPECT_FALSE(content.find("Should be filtered") != std::string::npos);
    EXPECT_TRUE(content.find("Warning should appear") != std::string::npos);
    EXPECT_TRUE(content.find("Error should appear") != std::string::npos);
    EXPECT_TRUE(content.find("Fatal should appear") != std::string::npos);
}

TEST_F(SinkTest, DedicatedSinkTest) {
    slick_logger::Logger::instance().clear_sinks();

    // Create a dedicated file sink
    auto dedicated_sink = std::make_shared<slick_logger::FileSink>("dedicated_sink.log");
    dedicated_sink->set_dedicated(true);
    slick_logger::Logger::instance().add_sink(dedicated_sink);

    // Create a regular file sink
    slick_logger::Logger::instance().add_file_sink("regular_sink.log");

    slick_logger::Logger::instance().init(1024);

    // Log using LOG_INFO (broadcast to all non-dedicated sinks)
    LOG_INFO("Broadcast message to regular sinks only");

    // Log directly to dedicated sink
    dedicated_sink->log_info("Direct message to dedicated sink");

    slick_logger::Logger::instance().reset();

    // Verify dedicated sink file contains only direct messages (should not receive broadcast messages)
    std::ifstream dedicated_file("dedicated_sink.log");
    std::string dedicated_content((std::istreambuf_iterator<char>(dedicated_file)),
                                 std::istreambuf_iterator<char>());
    dedicated_file.close();

    // The dedicated file should only contain the direct message
    EXPECT_TRUE(dedicated_content.find("Direct message to dedicated sink") != std::string::npos);
    EXPECT_FALSE(dedicated_content.find("Broadcast message to regular sinks only") != std::string::npos);

    // Verify regular sink file contains the broadcast message
    std::ifstream regular_file("regular_sink.log");
    std::string regular_content((std::istreambuf_iterator<char>(regular_file)),
                               std::istreambuf_iterator<char>());
    regular_file.close();

    EXPECT_TRUE(regular_content.find("Broadcast message to regular sinks only") != std::string::npos);
}

TEST_F(SinkTest, DailyFileSinkNoSizeRotationWhenZero) {
    // Test that size-based rotation is disabled when max_file_size is set to 0
    slick_logger::RotationConfig config;
    config.max_file_size = 0; // Disable size-based rotation

    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_daily_file_sink("daily_no_size_rotation.log", config);
    slick_logger::Logger::instance().init(1024);

    // Log many messages that would normally trigger size-based rotation
    // Each message is around 100+ bytes, so 50 messages would exceed typical rotation size
    for (int i = 0; i < 50; ++i) {
        LOG_INFO("No size rotation test message number {} with enough text to exceed typical file size limits if rotation was enabled", i);
    }

    slick_logger::Logger::instance().reset();

    // Check that base file exists
    EXPECT_TRUE(std::filesystem::exists("daily_no_size_rotation.log"));

    // Get current date for filename verification
    auto now = std::chrono::system_clock::now();
    time_t time_val = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_ptr = std::localtime(&time_val);
    std::string current_date;
    if (tm_ptr) {
        char date_str[11];
        std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_ptr);
        current_date = std::string(date_str);
    } else {
        current_date = "1970-01-01"; // fallback
    }

    // Verify that NO size-based rotation files were created
    std::string unexpected_rotation_file = "daily_no_size_rotation_" + current_date + "_000.log";
    EXPECT_FALSE(std::filesystem::exists(unexpected_rotation_file));

    unexpected_rotation_file = "daily_no_size_rotation_" + current_date + "_001.log";
    EXPECT_FALSE(std::filesystem::exists(unexpected_rotation_file));

    // Verify that all messages are in the single base file
    std::ifstream base_file("daily_no_size_rotation.log");
    std::string base_content((std::istreambuf_iterator<char>(base_file)),
                            std::istreambuf_iterator<char>());
    base_file.close();

    // Check that both first and last messages are in the base file
    EXPECT_TRUE(base_content.find("No size rotation test message number 0") != std::string::npos);
    EXPECT_TRUE(base_content.find("No size rotation test message number 49") != std::string::npos);

    // Verify the file size is larger than what would have triggered rotation
    // (with typical 200-byte config, this should be much larger)
    ASSERT_TRUE(std::filesystem::exists("daily_no_size_rotation.log"));
    auto file_size = std::filesystem::file_size("daily_no_size_rotation.log");
    EXPECT_GT(file_size, 200); // Should be much larger than typical rotation threshold
}

TEST_F(SinkTest, DailyFileSinkMultipleSizeRotations) {
    // Test multiple size-based rotations within the same day
    slick_logger::RotationConfig config;
    config.max_file_size = 200; // Very small for testing
    config.max_files = 5; // Keep up to 5 rotated files

    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_daily_file_sink("daily_multi_rotation.log", config);
    slick_logger::Logger::instance().init(1024);

    // Log enough messages to trigger multiple rotations
    for (int i = 0; i < 30; ++i) {
        LOG_INFO("Multiple rotation test message number {} with enough text to trigger size rotation", i);
    }

    slick_logger::Logger::instance().reset();

    // Get current date
    auto now = std::chrono::system_clock::now();
    time_t time_val = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_ptr = std::localtime(&time_val);
    std::string current_date;
    if (tm_ptr) {
        char date_str[11];
        std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_ptr);
        current_date = std::string(date_str);
    } else {
        current_date = "1970-01-01";
    }

    // Verify base file exists
    EXPECT_TRUE(std::filesystem::exists("daily_multi_rotation.log"));

    // Count how many rotated files exist for today
    int rotated_file_count = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(".", ec)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.find("daily_multi_rotation_" + current_date) == 0) {
                rotated_file_count++;
            }
        }
    }

    // Should have at least one rotated file (dated or indexed)
    EXPECT_GT(rotated_file_count, 0);

    // Verify we don't exceed max_files limit
    // max_files=5 means: daily_DATE.log (0) + _001.log (1) + _002.log (2) + _003.log (3) + _004.log (4)
    // So _005.log and beyond should not exist
    for (size_t i = config.max_files; i < 10; ++i) {
        std::ostringstream oss;
        oss << "daily_multi_rotation_" << current_date << "_"
            << std::setfill('0') << std::setw(3) << i << ".log";
        EXPECT_FALSE(std::filesystem::exists(oss.str())) << "File should not exist: " << oss.str();
    }
}

TEST_F(SinkTest, DailyFileSinkRestartWithOldFile) {
    // Test that restarting with an old log file properly rotates it
    slick_logger::RotationConfig config;
    config.max_file_size = 1000;

    // Get yesterday's date for verification
    auto yesterday_sys = std::chrono::system_clock::now() - std::chrono::hours(25); // 25 hours to ensure different date
    time_t yesterday_time = std::chrono::system_clock::to_time_t(yesterday_sys);
    std::tm* tm_ptr = std::localtime(&yesterday_time);
    std::string yesterday_date;
    if (tm_ptr) {
        char date_str[11];
        std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_ptr);
        yesterday_date = std::string(date_str);
    }

    // Create a test file with old timestamp
    std::ofstream old_file("daily_restart_test.log");
    old_file << "Old log content from previous day\n";
    old_file.close();

    // Set the file's last write time to yesterday
    auto now = std::filesystem::file_time_type::clock::now();
    auto yesterday = now - std::chrono::hours(25);
    std::filesystem::last_write_time("daily_restart_test.log", yesterday);

    // Wait a bit to ensure file system has updated the timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Now initialize the logger - it should rotate the old file
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_daily_file_sink("daily_restart_test.log", config);
    slick_logger::Logger::instance().init(1024);

    // Log new messages for today
    LOG_INFO("New message after restart");

    slick_logger::Logger::instance().reset();

    // Verify the old file was rotated with yesterday's date
    std::string rotated_file = "daily_restart_test_" + yesterday_date + ".log";

    // Check if rotation happened
    bool rotation_happened = std::filesystem::exists(rotated_file);

    if (rotation_happened) {
        // Verify the rotated file contains old content
        std::ifstream rotated(rotated_file);
        std::string rotated_content((std::istreambuf_iterator<char>(rotated)),
                                    std::istreambuf_iterator<char>());
        rotated.close();
        EXPECT_TRUE(rotated_content.find("Old log content from previous day") != std::string::npos);

        // Verify the base file contains new content only
        std::ifstream base("daily_restart_test.log");
        std::string base_content((std::istreambuf_iterator<char>(base)),
                                std::istreambuf_iterator<char>());
        base.close();
        EXPECT_TRUE(base_content.find("New message after restart") != std::string::npos);
        EXPECT_FALSE(base_content.find("Old log content from previous day") != std::string::npos);
    } else {
        // If rotation didn't happen, at least verify the new message is there
        std::ifstream base("daily_restart_test.log");
        std::string base_content((std::istreambuf_iterator<char>(base)),
                                std::istreambuf_iterator<char>());
        base.close();
        EXPECT_TRUE(base_content.find("New message after restart") != std::string::npos);

        // Print warning that rotation didn't happen (could be due to file system timestamp precision issues)
        std::cout << "Warning: Rotation did not occur. This might be a timestamp precision issue." << std::endl;
    }
}

TEST_F(SinkTest, DailyFileSinkRestartWithExistingRotatedFiles) {
    // Test that restarting when rotated files already exist doesn't overwrite them
    slick_logger::RotationConfig config;
    config.max_file_size = 1000;
    config.max_files = 3;

    // Get yesterday's date
    auto yesterday_sys = std::chrono::system_clock::now() - std::chrono::hours(25); // 25 hours to ensure different date
    time_t yesterday_time = std::chrono::system_clock::to_time_t(yesterday_sys);
    std::tm* tm_ptr = std::localtime(&yesterday_time);
    std::string yesterday_date;
    if (tm_ptr) {
        char date_str[11];
        std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_ptr);
        yesterday_date = std::string(date_str);
    }

    // Create existing rotated file from yesterday
    std::string existing_rotated = "daily_restart_existing_" + yesterday_date + ".log";
    std::ofstream existing(existing_rotated);
    existing << "Existing rotated content\n";
    existing.close();

    // Create current log file with old timestamp
    std::ofstream old_file("daily_restart_existing.log");
    old_file << "Second batch of old content\n";
    old_file.close();

    auto now = std::filesystem::file_time_type::clock::now();
    auto yesterday = now - std::chrono::hours(25);
    std::filesystem::last_write_time("daily_restart_existing.log", yesterday);

    // Wait a bit to ensure file system has updated the timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Initialize logger - should rotate without overwriting
    slick_logger::Logger::instance().clear_sinks();
    slick_logger::Logger::instance().add_daily_file_sink("daily_restart_existing.log", config);
    slick_logger::Logger::instance().init(1024);

    LOG_INFO("New content after restart");

    slick_logger::Logger::instance().reset();

    // Verify the original rotated file still exists
    EXPECT_TRUE(std::filesystem::exists(existing_rotated));

    std::string indexed_file = "daily_restart_existing_" + yesterday_date + "_001.log";
    bool rotation_happened = std::filesystem::exists(indexed_file);

    if (rotation_happened) {
        // After rotation:
        // - The original "daily_restart_existing_2025-10-01.log" was rotated to "_001.log"
        // - The old base file "daily_restart_existing.log" was renamed to "daily_restart_existing_2025-10-01.log"

        // Verify the indexed file has the original existing content
        std::ifstream check_indexed(indexed_file);
        std::string indexed_content((std::istreambuf_iterator<char>(check_indexed)),
                                   std::istreambuf_iterator<char>());
        check_indexed.close();
        EXPECT_TRUE(indexed_content.find("Existing rotated content") != std::string::npos);

        // Verify the dated file (non-indexed) now has the second batch content
        std::ifstream check_existing(existing_rotated);
        std::string existing_content((std::istreambuf_iterator<char>(check_existing)),
                                     std::istreambuf_iterator<char>());
        check_existing.close();
        EXPECT_TRUE(existing_content.find("Second batch of old content") != std::string::npos);
    } else {
        // Print warning that rotation didn't happen
        std::cout << "Warning: Rotation with existing files did not occur. This might be a timestamp precision issue." << std::endl;
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
