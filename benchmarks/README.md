# SlickLogger Benchmark Suite

A comprehensive performance benchmarking suite for comparing SlickLogger against other popular C++ logging libraries.

## Overview

This benchmark suite provides detailed performance analysis across multiple dimensions:

- **Throughput**: Messages per second under various thread loads
- **Latency**: Time per individual log call (nanosecond precision)  
- **Memory Usage**: Peak memory consumption and efficiency
- **Scalability**: Performance scaling with thread count
- **Burst Performance**: Handling of sudden logging spikes
- **Resource Monitoring**: CPU usage and memory tracking

## Compared Libraries

- **SlickLogger**: Our lock-free async logger with deferred formatting
- **spdlog (sync)**: Popular synchronous logging library  
- **spdlog (async)**: spdlog's asynchronous mode
- **std::ofstream**: Baseline direct file output with fmt formatting
- **fmt library**: Direct fmt::print to file (when available)

## Quick Start

### Prerequisites

- CMake 3.20 or later
- C++20 compatible compiler
- Internet connection (to download dependencies)

### Building

```bash
# Clone and build with benchmarks enabled
git clone <your-repo-url>
cd slick_logger
mkdir build && cd build

# Enable benchmarks during cmake configuration
cmake -DBUILD_BENCHMARKS=ON ..

# Build all benchmarks
cmake --build . --config Release

# Or build specific benchmarks
cmake --build . --target slick_logger_benchmark
cmake --build . --target latency_benchmark  
cmake --build . --target throughput_benchmark
cmake --build . --target memory_benchmark
```

### Running Benchmarks

```bash
# Run comprehensive benchmark suite
./slick_logger_benchmark

# Run with custom parameters (iterations, runs)  
./slick_logger_benchmark 100000 5

# Run individual benchmark suites
./latency_benchmark
./throughput_benchmark
./memory_benchmark
```

## Benchmark Programs

### 1. slick_logger_benchmark (Main Suite)

Comprehensive comparison across all libraries and scenarios:

```bash
./slick_logger_benchmark [iterations] [runs]
```

**Parameters:**
- `iterations`: Number of log calls per test (default: 50,000)
- `runs`: Number of test runs for statistical averaging (default: 3)

**Test Scenarios:**
- Single-threaded throughput (small, medium, large messages)
- Multi-threaded throughput (1, 2, 4, 8 threads)
- Latency measurements (nanosecond precision)
- Memory usage analysis
- Mixed message size performance

**Sample Output:**
```
=== THROUGHPUT BENCHMARKS ===

--- Small Messages ---

Testing with 1 thread(s):
Library               Mean      Median     P95       P99         StdDev
--------------------------------------------------------------------------------
slick_logger_small   2847291   2856543   2801234   2745123        45.2
spdlog_async_small   1934521   1945123   1876543   1823451        52.1
spdlog_sync_small     876543    881234    845123    823456        28.9
std_ofstream_small    654321    661234    634567    612345        31.4

Unit: ops/sec
```

### 2. latency_benchmark (Detailed Latency Analysis)

In-depth latency measurement with distribution analysis:

```bash
./latency_benchmark
```

**Features:**
- Individual call latency measurement
- Latency distribution histograms  
- Timeline analysis (warmup effects)
- Queue pressure impact testing
- P99.9 latency percentiles

**Sample Output:**
```
=== DETAILED LATENCY ANALYSIS ===

Samples: 10000
Mean:    342.5 ns
Median:  298.0 ns
P95:     567.0 ns
P99:     845.0 ns
P99.9:   1234.0 ns

Latency Distribution:
0-100ns     :    156 (1.6%)
100-500ns   :   8234 (82.3%)
500ns-1μs   :   1456 (14.6%)
1-5μs       :    145 (1.5%)
>5μs        :      9 (0.1%)
```

### 3. throughput_benchmark (Scaling Analysis)

Detailed throughput testing with thread scaling analysis:

```bash
./throughput_benchmark
```

**Features:**
- Thread scaling from 1 to 16 threads
- Efficiency analysis (scaling effectiveness)
- Burst performance testing
- CPU and memory monitoring during tests
- Contention analysis

**Sample Output:**
```
=== SCALING ANALYSIS ===
Logger           Threads  Throughput    CPU %  Memory MB   Efficiency
---------------------------------------------------------------------------
SlickLogger            1    2847291      12.3          8       100.0%
SlickLogger            2    5234567      23.1         12        91.9%
SlickLogger            4   10123456      43.2         18        88.9%
SlickLogger            8   18456789      78.4         28        81.2%
```

### 4. memory_benchmark (Memory Analysis)

Comprehensive memory usage analysis:

```bash
./memory_benchmark
```

**Features:**
- Peak memory consumption measurement
- Memory-per-message efficiency
- Queue size impact analysis
- Sustained load memory stability
- Memory fragmentation detection
- Memory leak detection

**Sample Output:**
```
=== MEMORY USAGE COMPARISON ===
Logger         Queue Size   Peak MB   Bytes/Msg   Efficiency
--------------------------------------------------------------
SlickLogger          1024        12        23.4         4274
SlickLogger          8192        45        18.2         5495
spdlog_async         1024        18        34.1         2932
spdlog_async         8192        67        27.3         3663

Efficiency = Messages per MB of memory used
```

## Benchmark Configuration

### Environment Variables

```bash
# Set CPU affinity for consistent results
export SLICK_BENCHMARK_CPU_AFFINITY=0,1,2,3

# Set output directory  
export SLICK_BENCHMARK_OUTPUT_DIR=./benchmark_results

# Enable detailed output
export SLICK_BENCHMARK_VERBOSE=1
```

### Compile-Time Options

```cmake
# Enable additional debug information in benchmarks
-DBENCHMARK_DEBUG_OUTPUT=ON

# Use system spdlog instead of fetching
-DUSE_SYSTEM_SPDLOG=ON

# Disable specific benchmarks
-DENABLE_MEMORY_BENCHMARKS=OFF
```

## Understanding Results

### Throughput Metrics

- **ops/sec**: Operations (log calls) per second
- Higher values indicate better performance
- Linear scaling with threads indicates good parallelization
- Compare against baseline (std::ofstream) for context

### Latency Metrics  

- **Mean/Median**: Average response time per call
- **P95/P99**: 95th and 99th percentile latencies
- **P99.9**: Critical for understanding tail latencies
- Lower values indicate better responsiveness

### Memory Metrics

- **Peak Memory**: Maximum memory used during test
- **Bytes/Message**: Memory efficiency indicator  
- **Efficiency Score**: Messages processed per MB
- Lower peak memory and bytes/message is better

### Scaling Efficiency

```
Efficiency = (Throughput_N_threads / Throughput_1_thread) / N_threads * 100%
```

- 100% = perfect linear scaling
- >80% = good scaling
- <60% = poor scaling, likely contention issues

## Optimization Tips

Based on benchmark results:

### For High Throughput
- Use async logging libraries (SlickLogger, spdlog async)
- Increase queue size for better batching
- Consider multiple writer threads for very high loads

### For Low Latency  
- Prefer SlickLogger's deferred formatting
- Use smaller queue sizes to reduce memory pressure
- Consider sync logging for ultra-low latency requirements

### For Memory Efficiency
- Tune queue sizes based on expected load
- Monitor for memory fragmentation in long-running applications
- Consider message size impact on memory usage

## Troubleshooting

### Common Issues

**Build Errors:**
```bash
# Missing dependencies
sudo apt-get install build-essential cmake

# C++20 support issues  
export CXX=g++-10  # or clang++-12
```

**Runtime Issues:**
```bash
# Permission errors with log files
mkdir -p benchmark_logs
chmod 755 benchmark_logs

# Memory allocation failures
ulimit -v unlimited
```

**Inconsistent Results:**
```bash
# Disable CPU frequency scaling
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Disable hyperthreading if needed
echo off | sudo tee /sys/devices/system/cpu/smt/control
```

### Performance Tuning

**For Accurate Benchmarks:**
- Run in Release mode only (`-DCMAKE_BUILD_TYPE=Release`)
- Close unnecessary applications
- Use CPU affinity to pin threads
- Run multiple iterations and average results
- Consider system warm-up time

**System Configuration:**
```bash
# Increase file descriptor limits
ulimit -n 65536

# Increase memory limits
ulimit -m unlimited

# Set process priority
nice -n -10 ./slick_logger_benchmark
```

## Extending Benchmarks

### Adding New Libraries

1. Create benchmark scenario class inheriting from `ThroughputScenario` or `LatencyScenario`
2. Implement logger-specific setup/cleanup and log_message methods
3. Add to benchmark runner in appropriate test functions
4. Update CMakeLists.txt with new dependencies

### Custom Test Scenarios

```cpp
// Example custom scenario
class CustomThroughputScenario : public ThroughputScenario<YourLogger> {
public:
    CustomThroughputScenario(size_t threads) 
        : ThroughputScenario(nullptr, "your_logger", threads) {}
    
    void setup() override {
        // Initialize your logger
    }
    
    void log_message() override {
        // Call your logger's log function
    }
};
```

## Contributing

When adding new benchmarks or improvements:

1. Follow existing code structure and naming conventions
2. Add appropriate statistical analysis
3. Include documentation for new test scenarios  
4. Ensure cross-platform compatibility
5. Add example output to documentation

## License

This benchmark suite is part of SlickLogger and follows the same MIT license.