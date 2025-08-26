#pragma once

#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <random>
#include "benchmark_utils.hpp"

namespace benchmark_scenarios {

using namespace benchmark_utils;

// Base test scenario interface
class ITestScenario {
public:
    virtual ~ITestScenario() = default;
    virtual std::string name() const = 0;
    virtual void setup() = 0;
    virtual void run(size_t iterations) = 0;
    virtual void cleanup() = 0;
    virtual std::string get_measurement_unit() const { return "ops/sec"; }
};

// Template for logger-specific scenarios
template<typename LoggerType>
class LoggerTestScenario : public ITestScenario {
public:
    LoggerTestScenario(std::shared_ptr<LoggerType> logger, const std::string& name)
        : logger_(logger), name_(name), msg_gen_() {}

protected:
    std::shared_ptr<LoggerType> logger_;
    std::string name_;
    MessageGenerator msg_gen_;
};

// Throughput test - measures messages per second
template<typename LoggerType>
class ThroughputScenario : public LoggerTestScenario<LoggerType> {
public:
    ThroughputScenario(std::shared_ptr<LoggerType> logger, const std::string& name, 
                      size_t num_threads = 1)
        : LoggerTestScenario<LoggerType>(logger, name + "_throughput_" + std::to_string(num_threads) + "t")
        , num_threads_(num_threads) {}

    void setup() override {
        // Logger-specific setup will be done in derived classes
    }

    void run(size_t iterations) override {
        if (num_threads_ == 1) {
            run_single_threaded(iterations);
        } else {
            run_multi_threaded(iterations);
        }
    }

    void cleanup() override {
        // Logger-specific cleanup
    }

protected:
    virtual void log_message() = 0;
    
    void run_single_threaded(size_t iterations) {
        for (size_t i = 0; i < iterations; ++i) {
            log_message();
        }
    }

    void run_multi_threaded(size_t iterations) {
        std::atomic<size_t> counter{0};
        std::vector<std::thread> threads;
        ThreadBarrier barrier(num_threads_);
        
        size_t iterations_per_thread = iterations / num_threads_;
        
        for (size_t t = 0; t < num_threads_; ++t) {
            threads.emplace_back([this, &barrier, iterations_per_thread]() {
                barrier.wait(); // Synchronize thread starts
                
                for (size_t i = 0; i < iterations_per_thread; ++i) {
                    log_message();
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }

    size_t num_threads_;
};

// Latency test - measures time per log call
template<typename LoggerType>
class LatencyScenario : public LoggerTestScenario<LoggerType> {
public:
    LatencyScenario(std::shared_ptr<LoggerType> logger, const std::string& name)
        : LoggerTestScenario<LoggerType>(logger, name + "_latency") {}

    void setup() override {}

    void run(size_t iterations) override {
        latencies_.clear();
        latencies_.reserve(iterations);

        for (size_t i = 0; i < iterations; ++i) {
            Timer timer;
            log_message();
            latencies_.push_back(timer.elapsed_ns());
        }
    }

    void cleanup() override {}

    std::string get_measurement_unit() const override { return "ns/op"; }

    const std::vector<double>& get_latencies() const { return latencies_; }

protected:
    virtual void log_message() = 0;
    std::vector<double> latencies_;
};

// Message size test scenarios
enum class MessageSize {
    SMALL,   // ~20 bytes
    MEDIUM,  // ~100-200 bytes  
    LARGE    // ~500-1000 bytes
};

// Realistic application scenarios
class RealisticScenarios {
public:
    // Web server logging scenario
    struct WebServerScenario {
        std::vector<std::string> request_templates = {
            "GET /api/users/{} - 200 OK - {} ms - IP: {} - User-Agent: {}",
            "POST /api/auth/login - 401 Unauthorized - {} ms - IP: {} - Reason: {}",
            "PUT /api/data/{} - 500 Internal Error - {} ms - IP: {} - Error: {}",
            "DELETE /api/resource/{} - 204 No Content - {} ms - IP: {} - User: {}"
        };
        
        std::string generate_message(MessageGenerator& gen) {
            auto& tmpl = request_templates[gen.random_int() % request_templates.size()];
            // Format with random values - this would be done by the actual logger
            return tmpl;
        }
    };

    // Database operation scenario  
    struct DatabaseScenario {
        std::vector<std::string> query_templates = {
            "Query executed: SELECT * FROM users WHERE id = {} - {} rows returned in {} ms",
            "Transaction started: ID {} - {} operations - Isolation level: {}",
            "Index rebuild: Table {} - {} entries processed in {} seconds",
            "Backup operation: Database {} - {} GB backed up to {} in {} minutes"
        };
        
        std::string generate_message(MessageGenerator& gen) {
            auto& tmpl = query_templates[gen.random_int() % query_templates.size()];
            return tmpl;
        }
    };

    // Mixed severity logging scenario
    struct MixedSeverityScenario {
        struct LogEvent {
            std::string message;
            int severity; // 0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=fatal
            double probability;
        };
        
        std::vector<LogEvent> events = {
            {"Trace: Function {} called with parameters {}", 0, 0.05},    // 5% trace
            {"Debug: Variable {} = {} at line {}", 1, 0.15},              // 15% debug  
            {"Info: User {} performed action {} successfully", 2, 0.60},  // 60% info
            {"Warning: Rate limit approached for user {} - {}/hour", 3, 0.15}, // 15% warn
            {"Error: Failed to process request {} - {}", 4, 0.04},        // 4% error
            {"Fatal: System shutdown initiated - {}", 5, 0.01}            // 1% fatal
        };
        
        LogEvent select_event(MessageGenerator& gen) {
            double r = gen.random_double() / 1000.0; // 0-1 range
            double cumulative = 0.0;
            
            for (const auto& event : events) {
                cumulative += event.probability;
                if (r <= cumulative) {
                    return event;
                }
            }
            return events.back(); // Fallback
        }
    };
};

// Stress test scenario - high frequency logging
template<typename LoggerType>
class StressTestScenario : public LoggerTestScenario<LoggerType> {
public:
    StressTestScenario(std::shared_ptr<LoggerType> logger, const std::string& name,
                      size_t num_threads, size_t duration_seconds)
        : LoggerTestScenario<LoggerType>(logger, name + "_stress")
        , num_threads_(num_threads)
        , duration_seconds_(duration_seconds) {}

    void setup() override {}

    void run(size_t iterations) override {
        std::atomic<size_t> total_messages{0};
        std::atomic<bool> running{true};
        std::vector<std::thread> threads;

        // Start timer thread
        std::thread timer_thread([&running, this]() {
            std::this_thread::sleep_for(std::chrono::seconds(duration_seconds_));
            running = false;
        });

        // Start logging threads
        for (size_t i = 0; i < num_threads_; ++i) {
            threads.emplace_back([this, &running, &total_messages]() {
                while (running) {
                    log_message();
                    total_messages++;
                }
            });
        }

        timer_thread.join();
        for (auto& thread : threads) {
            thread.join();
        }

        total_messages_ = total_messages.load();
    }

    void cleanup() override {}

    size_t get_total_messages() const { return total_messages_; }

protected:
    virtual void log_message() = 0;
    
    size_t num_threads_;
    size_t duration_seconds_;
    size_t total_messages_ = 0;
};

} // namespace benchmark_scenarios