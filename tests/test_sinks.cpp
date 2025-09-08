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
            "daily_size_test.log"
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
                if (filename.find("daily_test_") == 0 || filename.find("daily_size_test_") == 0) {
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
    config.queue_size = 2048;
    
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

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
