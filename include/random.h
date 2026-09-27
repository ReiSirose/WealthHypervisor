#pragma once
#include <cstdint>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

class Xoshiro256PlusPlus {
private:
    uint64_t s[4]{};

    static inline uint64_t rotl(const uint64_t x, int k) noexcept {
        return (x << k) | (x >> (64 - k));
    }

    static inline uint64_t splitmix64(uint64_t& state) noexcept {
        state += 0x9E3779B97F4A7C15ULL;
        uint64_t value = state;
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31);
    }

public:
    using result_type = uint64_t;

    explicit Xoshiro256PlusPlus(uint64_t seed = 1337) noexcept {
        this->seed(seed);
    }

    void seed(uint64_t seed_value) noexcept {
        for (uint64_t& state : s) {
            state = splitmix64(seed_value);
        }
        if ((s[0] | s[1] | s[2] | s[3]) == 0) {
            s[0] = 1;
        }
    }

    static constexpr result_type min() noexcept { return 0; }
    static constexpr result_type max() noexcept { return std::numeric_limits<result_type>::max(); }

    inline uint64_t operator()() noexcept {
        const uint64_t result = rotl(s[0] + s[3], 23) + s[0];
        const uint64_t t = s[1] << 17;

        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];

        s[2] ^= t;
        s[3] = rotl(s[3], 45);

        return result;
    }

    inline double next_double() noexcept {
        return (operator()() >> 11) * (1.0 / (1ULL << 53));
    }
};

class NormalRandomPool {
private:
    std::vector<double> pool;
    size_t cursor{0};
    size_t batch_size{1024};

public:
    explicit NormalRandomPool(size_t requested_batch_size = 1024)
        : batch_size(requested_batch_size < 2 ? 2 : requested_batch_size + (requested_batch_size & 1U)) {}

    void reset() noexcept {
        pool.clear();
        cursor = 0;
    }

    void refill(Xoshiro256PlusPlus& rng) {
        pool.resize(batch_size);
        cursor = 0;

        for (size_t i = 0; i < batch_size; i += 2) {
            double u1 = rng.next_double();
            double u2 = rng.next_double();
            while (u1 <= 1e-15) u1 = rng.next_double();

            double radius = std::sqrt(-2.0 * std::log(u1));
            double theta = 2.0 * 3.14159265358979323846 * u2;

            pool[i] = radius * std::cos(theta);
            pool[i + 1] = radius * std::sin(theta);
        }
    }

    inline double get_next(Xoshiro256PlusPlus& rng) {
        if (cursor >= pool.size()) refill(rng);
        return pool[cursor++];
    }
};