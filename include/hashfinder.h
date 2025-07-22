#pragma once

#include <string>
#include <functional>
#include <cstdint>

#define HASHFINDER_API

namespace HashFinder {

    // Progress callback type: (current_progress, total_range, rate_per_sec, eta_seconds) -> void
    using ProgressCallback = std::function<void(uint64_t, uint64_t, double, double)>;

    struct Config {
        uint64_t start_range = 0;              // Start of search range
        uint64_t end_range = 1000000000ULL;    // End of search range  
        uint32_t progress_interval_ms = 2000;  // Progress callback interval
        bool use_all_threads = true;           // Use all available CPU threads
        uint32_t thread_count = 0;             // Manual thread count (0 = auto)
    };

    struct Result {
        bool found = false;
        uint64_t value = 0;
        std::string digits;
        uint64_t search_time_ms = 0;
        uint64_t total_checked = 0;
    };

    // Library functions
    HASHFINDER_API bool is_avx512_available();
    HASHFINDER_API uint32_t fnv1a_hash_digits(uint64_t number);
    HASHFINDER_API Result find_hash_match(uint32_t target_hash, const Config& config = {}, ProgressCallback callback = nullptr);
    HASHFINDER_API std::string format_digits(uint64_t number, int width = 9);
}
