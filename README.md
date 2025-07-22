# HashFinderAVX512

A high-performance hash brute-forcing library optimized for AVX-512 capable CPUs. Designed for reverse engineering, crackme challenges, and keygen development.

## Features

- **AVX-512 Optimized**: Process 16 hashes simultaneously using SIMD instructions
- **Multi-threaded**: Fully parallel execution across all CPU cores  

## Purpose

This library is designed for RE, originally made as a helper lib to speed up crackme keygens.

## Supported Hash Algorithms

| Algorithm | Input Type | Output | Status |
|-----------|------------|---------|--------|
| **FNV-1a 32-bit** | Numeric digits | `uint32_t` | Implemented |


## Performance

**Tested on AMD Ryzen 9950X (32 threads, AVX-512):**
- **~66 million hashes/second** for FNV-1a
- **Search 1 billion combinations in ~15 seconds**
- **Linear scaling** with CPU core count

## 🛠️ Requirements

**CPU Requirements:**
- **AVX-512F** instruction set (Intel Skylake-X+, AMD Zen 4+)

## Building

### Clone Repository
```bash
git clone https://github.com/altidias/HashFinderAVX512.git
cd HashFinderAVX512
```

### Build Library
```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Install 
cmake --install build --prefix install
```

### Windows (Visual Studio)
```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

## Usage

### Example
```cpp
#include "hashfinder.h"
#include <iostream>

int main() {
    using namespace HashFinder;
    
    // Configure search
    Config config;
    config.start_range = 0;
    config.end_range = 1000000000ULL;  // Search 1 billion combinations
    
    // Progress callback
    auto progress = [](uint64_t current, uint64_t total, double rate, double eta) {
        std::cout << "Progress: " << (100.0 * current / total) << "% - "
                  << static_cast<uint64_t>(rate) << " h/s" << std::endl;
    };
    
    // Search for target hash
    uint32_t target_hash = 0xf9b9b765;
    Result result = find_hash_match(target_hash, config, progress);
    
    if (result.found) {
        std::cout << "Found: " << result.digits << std::endl;
        std::cout << "Time: " << result.search_time_ms << "ms" << std::endl;
    }
    
    return 0;
}
```

### Integration with CMake
```cmake
find_package(HashFinderAVX512 REQUIRED)
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE HashFinder::AVX512)
```

## API Reference

### Core Functions
```cpp
// Check AVX-512 availability
bool is_avx512_available();

// Compute FNV-1a hash of a number
uint32_t fnv1a_hash_digits(uint64_t number);

// Main search function
Result find_hash_match(uint32_t target_hash, const Config& config, ProgressCallback callback);
```

### Configuration
```cpp
struct Config {
    uint64_t start_range = 0;              // Search start
    uint64_t end_range = 1000000000ULL;    // Search end
    uint32_t progress_interval_ms = 2000;  // Progress update frequency
    bool use_all_threads = true;           // Use all CPU cores
    uint32_t thread_count = 0;             // Manual thread count
};
```

### Results
```cpp
struct Result {
    bool found = false;           // Whether target was found
    uint64_t value = 0;          // Numeric value that produced hash
    std::string digits;          // Formatted digits with leading zeros
    uint64_t search_time_ms = 0; // Total search time
    uint64_t total_checked = 0;  // Total combinations tested
};
```

