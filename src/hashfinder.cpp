#include "hashfinder.h"
#include <iostream>
#include <chrono>
#include <atomic>
#include <thread>
#include <algorithm>
#include <execution>
#include <vector>
#include <array>
#include <immintrin.h>
#include <intrin.h>

namespace HashFinder {

    bool is_avx512_available() {
        int cpui[4];
        __cpuid(cpui, 7);
        return (cpui[1] & (1 << 16)) != 0; // Check AVX-512F bit
    }
    
    // Scalar FNV-1a hash for single number https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
    uint32_t fnv1a_hash_digits(uint64_t number) {
        uint32_t hash = 0x811c9dc5;
        char digits[20]{}; // Support up to 20 digits

        // Extract digits
        uint64_t temp = number;
        int digit_count = 0;
        do {
            digits[digit_count++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp > 0);

        // Hash in reverse order (most significant first)
        for (int i = digit_count - 1; i >= 0; i--) {
            hash ^= static_cast<uint8_t>(digits[i]);
            hash *= 0x1000193;
        }

        return hash;
    }

    class AVX512Hasher {
    private:
        static constexpr size_t SIMD_WIDTH = 16;

    public:
        // Batch hash computation for 16 numbers
        std::array<uint32_t, SIMD_WIDTH> hash_batch(const std::array<uint64_t, SIMD_WIDTH>& numbers) const {
            // Initialize 16 hash values with FNV offset basis
            __m512i hashes = _mm512_set1_epi32(0x811c9dc5);
            const __m512i fnv_prime = _mm512_set1_epi32(0x1000193);

            // Find maximum number of digits needed
            int max_digits = 0;
            for (const auto& num : numbers) {
                uint64_t temp = num;
                int digits = 0;
                do {
                    digits++;
                    temp /= 10;
                } while (temp > 0);
                max_digits = std::max(max_digits, digits);
            }

            // Process each digit position
            for (int digit_pos = max_digits - 1; digit_pos >= 0; digit_pos--) {
                alignas(64) uint32_t digit_array[16]{};

                for (int i = 0; i < 16; i++) {
                    uint64_t temp = numbers[i];
                    for (int j = 0; j < digit_pos; j++) {
                        temp /= 10;
                    }
                    digit_array[i] = (temp > 0) ? ('0' + (temp % 10)) : 0;
                }

                __m512i digits = _mm512_load_si512(reinterpret_cast<const __m512i*>(digit_array));

                // Only process non-zero digits
                __mmask16 non_zero_mask = _mm512_cmpneq_epi32_mask(digits, _mm512_setzero_si512());

                // FNV-1a: hash ^= digit; hash *= prime (only for non-zero digits)
                hashes = _mm512_mask_xor_epi32(hashes, non_zero_mask, hashes, digits);
                hashes = _mm512_mask_mullo_epi32(hashes, non_zero_mask, hashes, fnv_prime);
            }
            std::array<uint32_t, SIMD_WIDTH> result{};
            _mm512_storeu_si512(reinterpret_cast<__m512i*>(result.data()), hashes);
            return result;
        }
    };

    Result find_hash_match(uint32_t target_hash, const Config& config, ProgressCallback callback) {
        Result result;

        if (!is_avx512_available()) {
            std::cerr << "AVX-512 not available on this CPU!" << std::endl;
            return result;
        }

        const uint64_t search_range = config.end_range - config.start_range;
        const size_t batch_size = 16; // AVX-512 width

        std::atomic<bool> found{ false };
        std::atomic<uint64_t> result_value{ 0 };
        std::atomic<uint64_t> progress_counter{ 0 };

        auto start_time = std::chrono::high_resolution_clock::now();
        AVX512Hasher hasher;

        // Determine thread count
        uint32_t thread_count = config.use_all_threads ?
            std::thread::hardware_concurrency() :
            (config.thread_count > 0 ? config.thread_count : std::thread::hardware_concurrency());

        // Progress reporting thread
        std::jthread progress_thread([&](std::stop_token token) {
            if (!callback) return;

            while (!token.stop_requested() && !found.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(config.progress_interval_ms));

                if (!found.load()) {
                    auto current_time = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
                    uint64_t current_progress = progress_counter.load();
                    double rate = static_cast<double>(current_progress) / (elapsed.count() / 1000.0);
                    double eta_seconds = (search_range - current_progress) / rate;

                    callback(current_progress, search_range, rate, eta_seconds);
                }
            }
            });

        // Create batch starting points
        std::vector<uint64_t> batch_starts;
        for (uint64_t i = config.start_range; i < config.end_range; i += batch_size) {
            batch_starts.push_back(i);
        }

        // Parallel search
        std::for_each(std::execution::par_unseq,
            batch_starts.begin(), batch_starts.end(),
            [&](uint64_t batch_start) {
                if (found.load()) return;

                // Prepare batch
                std::array<uint64_t, 16> batch{};
                for (size_t i = 0; i < 16; i++) {
                    batch[i] = batch_start + i;
                    if (batch[i] >= config.end_range) {
                        batch[i] = config.end_range - 1; // Clamp to range
                    }
                }

                // Compute hashes
                auto hashes = hasher.hash_batch(batch);

                // Check results
                for (size_t i = 0; i < 16; i++) {
                    if (batch_start + i >= config.end_range) break;

                    if (hashes[i] == target_hash && !found.exchange(true)) {
                        result_value.store(batch_start + i);
                        return;
                    }
                }

                // Update progress
                progress_counter.store(batch_start);
            });

        progress_thread.request_stop();

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Fill result
        result.search_time_ms = duration.count();
        result.total_checked = progress_counter.load();

        uint64_t final_value = result_value.load();
        if (final_value > 0 || (final_value == 0 && fnv1a_hash_digits(0) == target_hash)) {
            result.found = true;
            result.value = final_value;
            result.digits = format_digits(final_value);
        }

        return result;
    }

    // Utility function to format digits with leading zeros
    std::string format_digits(uint64_t number, int width) {
        std::string result = std::to_string(number);
        while (static_cast<int>(result.length()) < width) {
            result = "0" + result;
        }
        return result;
    }
}
