#include <slick_logger/logger.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>

TEST(SlickLoggerTest, BasicLogging) {
    // Clean up any existing log file
    std::filesystem::remove("test.log");

    // Initialize logger
    slick_logger::Logger::instance().init("test.log", 1024);

    // Log a message
    LOG_INFO("Test message");

    // Shutdown
    slick_logger::Logger::instance().shutdown();

    // Check if file was created and contains the message
    ASSERT_TRUE(std::filesystem::exists("test.log"));

    std::ifstream log_file("test.log");
    std::string line;
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("Test message") != std::string::npos);
}

TEST(SlickLoggerTest, LogFilter) {
    // Clean up any existing log file
    std::filesystem::remove("test.log");

    // Initialize logger
    slick_logger::Logger::instance().init("test.log", 1024);
    slick_logger::Logger::instance().set_log_level(slick_logger::LogLevel::INFO);

    LOG_INFO("Test message");
    LOG_DEBUG("This debug message should not appear");
    LOG_WARN("This is a warning");
    LOG_TRACE("This trace message should not appear");
    LOG_ERROR("This is an error");
    LOG_FATAL("This is fatal");

    // Shutdown
    slick_logger::Logger::instance().shutdown();

    // Check if file was created and contains the message
    ASSERT_TRUE(std::filesystem::exists("test.log"));

    std::ifstream log_file("test.log");
    std::string line;
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("Test message") != std::string::npos);
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("This is a warning") != std::string::npos);
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("This is an error") != std::string::npos);
    std::getline(log_file, line);
    EXPECT_TRUE(line.find("This is fatal") != std::string::npos);
}

TEST(SlickLoggerTest, MultiThreadedLogging) {
    std::filesystem::remove("test_mt.log");

    slick_logger::Logger::instance().init("test_mt.log", 1024);

    // Log from multiple threads
    std::thread t1([]() {
        for (int i = 0; i < 5; ++i) {
            LOG_INFO("Thread 1: {}", i);
        }
    });

    std::thread t2([]() {
        for (int i = 0; i < 5; ++i) {
            LOG_INFO("Thread 2: {}", i);
        }
    });

    t1.join();
    t2.join();

    slick_logger::Logger::instance().shutdown();

    // Count lines in log file
    std::ifstream log_file("test_mt.log");
    std::string line;
    int count = 0;
    while (std::getline(log_file, line)) {
        count++;
    }
    EXPECT_EQ(count, 10); // 5 from each thread
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
