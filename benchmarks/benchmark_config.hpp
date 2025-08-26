#pragma once

// Benchmark configuration file
// Modify these values to customize benchmark behavior

namespace benchmark_config {

// Test iteration counts
constexpr size_t DEFAULT_WARMUP_ITERATIONS = 1000;
constexpr size_t DEFAULT_MEASUREMENT_ITERATIONS = 50000;
constexpr size_t DEFAULT_NUM_RUNS = 3;

// Quick test parameters (for faster development testing)
constexpr size_t QUICK_MEASUREMENT_ITERATIONS = 10000;  
constexpr size_t QUICK_NUM_RUNS = 2;

// Thread scaling test parameters
constexpr size_t MAX_THREAD_COUNT = 16;
static const std::vector<size_t> THREAD_COUNTS = {1, 2, 4, 8, 16};

// Queue size testing
constexpr size_t DEFAULT_QUEUE_SIZE = 65536;
static const std::vector<size_t> QUEUE_SIZES = {1024, 8192, 65536, 262144};

// Message size configuration
namespace messages {
    // Small message templates (~20-50 bytes)
    static const std::vector<std::string> SMALL_TEMPLATES = {
        "Info message {}",
        "Debug trace {}",
        "Warning occurred {}",
        "Error detected {}",
        "System event {}"
    };

    // Medium message templates (~100-200 bytes)
    static const std::vector<std::string> MEDIUM_TEMPLATES = {
        "Processing request {} with status {} in {} ms",
        "Database query returned {} rows for table {} in {} ms",
        "Network request to {} completed with code {} after {} ms",
        "File operation {} on {} completed in {} ms",
        "Cache operation {} for key {} completed with ratio {}"
    };

    // Large message templates (~500-1000 bytes)
    static const std::vector<std::string> LARGE_TEMPLATES = {
        "System report: CPU {}%, Memory {} MB, Disk {} GB, Network {} Mbps, "
        "Connections {}, Requests {}, Cache {}, DB {}, Queue {}, Error {} at {}",
        
        "Transaction {}: {} items, ${} total, customer {}, location {}, "
        "payment {}, confirmation {}, system {}, thread {}, priority {}, "
        "duration {} ms, validation {}, audit {}, partition {}"
    };
}

// Memory testing parameters
constexpr size_t MEMORY_TEST_DURATION_SECONDS = 30;
constexpr size_t MEMORY_MESSAGES_PER_SECOND = 10000;
constexpr size_t MEMORY_FRAGMENTATION_CYCLES = 10;
constexpr size_t MEMORY_FRAGMENTATION_MESSAGES_PER_CYCLE = 10000;

// Burst testing parameters  
constexpr size_t BURST_SIZE = 50000;
constexpr size_t NUM_BURSTS = 5;
constexpr size_t BURST_INTERVAL_MS = 1000;

// Latency testing parameters
constexpr size_t LATENCY_SAMPLE_COUNT = 10000;
constexpr size_t LATENCY_WARMUP_COUNT = 1000;

// Background load testing (for queue pressure analysis)
static const std::vector<size_t> BACKGROUND_LOAD_RATES = {0, 1000, 5000, 10000}; // msgs/sec

// Output configuration
constexpr bool ENABLE_DETAILED_OUTPUT = false;
constexpr bool ENABLE_CSV_OUTPUT = true;
constexpr bool ENABLE_JSON_OUTPUT = false;

// File output configuration
namespace output {
    static const std::string LOG_DIRECTORY = "benchmark_logs";
    static const std::string RESULTS_DIRECTORY = "benchmark_results";
    static const std::string CSV_FILENAME = "benchmark_results.csv";
    static const std::string JSON_FILENAME = "benchmark_results.json";
}

// Platform-specific configuration
#ifdef _WIN32
    constexpr bool USE_HIGH_PRIORITY_PROCESS = false; // Set to true if running as admin
    constexpr bool ENABLE_CPU_AFFINITY = false;       // Set core affinity
    static const std::vector<size_t> CPU_AFFINITY_MASK = {0, 1, 2, 3};
#else
    constexpr bool USE_NICE_PRIORITY = true;          // Use nice for process priority
    constexpr int NICE_PRIORITY = -10;                // Higher priority (requires sudo)
    constexpr bool ENABLE_CPU_AFFINITY = false;       // Set core affinity  
    static const std::vector<size_t> CPU_AFFINITY_MASK = {0, 1, 2, 3};
#endif

// Statistical analysis configuration
constexpr double OUTLIER_THRESHOLD_PERCENTILE = 99.9; // Remove outliers above this percentile
constexpr size_t MIN_SAMPLES_FOR_STATS = 10;          // Minimum samples for statistical analysis
constexpr double CONFIDENCE_INTERVAL = 95.0;          // Confidence interval for error bars

// Performance thresholds (for pass/fail analysis)
namespace thresholds {
    constexpr double MIN_THROUGHPUT_OPS_SEC = 100000;     // Minimum acceptable throughput
    constexpr double MAX_LATENCY_P99_US = 100.0;          // Maximum acceptable P99 latency
    constexpr size_t MAX_MEMORY_PER_MESSAGE_BYTES = 1024; // Maximum memory per message
    constexpr double MIN_SCALING_EFFICIENCY = 60.0;       // Minimum scaling efficiency %
}

// Benchmark feature flags
constexpr bool ENABLE_THROUGHPUT_TESTS = true;
constexpr bool ENABLE_LATENCY_TESTS = true;  
constexpr bool ENABLE_MEMORY_TESTS = true;
constexpr bool ENABLE_SCALING_TESTS = true;
constexpr bool ENABLE_BURST_TESTS = true;
constexpr bool ENABLE_FRAGMENTATION_TESTS = true;

// Library-specific configuration
namespace libraries {
    // SlickLogger configuration
    constexpr size_t SLICK_DEFAULT_QUEUE_SIZE = 65536;
    constexpr bool SLICK_ENABLE_ALL_SINKS = false;     // Test with multiple sink types
    
    // spdlog configuration  
    constexpr size_t SPDLOG_ASYNC_QUEUE_SIZE = 65536;
    constexpr size_t SPDLOG_ASYNC_THREADS = 1;
    constexpr bool SPDLOG_ENABLE_PATTERN_FORMATTING = true;
    
    // Baseline configuration
    constexpr bool BASELINE_ENABLE_BUFFERING = true;
    constexpr size_t BASELINE_BUFFER_SIZE = 8192;
}

// Environment detection and adjustment
inline void adjust_for_environment() {
    // Automatically adjust parameters based on system capabilities
    
#ifdef _DEBUG
    // Reduce iteration counts in debug mode
    const_cast<size_t&>(DEFAULT_MEASUREMENT_ITERATIONS) = 10000;
    const_cast<size_t&>(DEFAULT_NUM_RUNS) = 2;
#endif

    // Detect available memory and adjust test sizes accordingly
    // This could be implemented using platform-specific APIs
    
    // Detect CPU count and adjust thread test parameters
    size_t cpu_count = std::thread::hardware_concurrency();
    if (cpu_count > 0 && cpu_count < MAX_THREAD_COUNT) {
        // Adjust max thread count based on available cores
        // Implementation would modify THREAD_COUNTS vector
    }
}

} // namespace benchmark_config