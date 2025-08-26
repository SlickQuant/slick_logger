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
            "daily_test.log"
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
                if (filename.find("daily_test_") == 0) {
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
    
    // Should create a file with today's date
    auto now = std::chrono::system_clock::now();
    time_t time_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_val);
    
    char date_str[11];
    std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);
    
    std::string expected_filename = std::string("daily_test_") + date_str + ".log";
    EXPECT_TRUE(std::filesystem::exists(expected_filename));
}

TEST_F(SinkTest, BackwardsCompatibility) {
    // Test that old init method still works
    slick_logger::Logger::instance().init("console_test.log", 1024);
    
    LOG_INFO("Backwards compatibility test");
    
    slick_logger::Logger::instance().reset();
    
    ASSERT_TRUE(std::filesystem::exists("console_test.log"));
    
    std::ifstream log_file("console_test.log");
    std::string line;
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("Backwards compatibility test") != std::string::npos);
}

TEST_F(SinkTest, LogConfigTest) {
    slick_logger::LogConfig config;
    config.sinks.push_back(std::make_shared<slick_logger::ConsoleSink>(false, false));
    config.sinks.push_back(std::make_shared<slick_logger::FileSink>("multi_sink_test.log"));
    config.min_level = slick_logger::LogLevel::WARN;
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