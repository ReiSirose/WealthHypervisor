#pragma once

#include <cmath>
#include <cstdint>
#include "random.h"


class MarketEngine {
private:  
    double log_mean;

    Xoshiro256PlusPlus rng;
    NormalRandomPool normal_pool;
    double annual_volatility;

public:
    explicit MarketEngine(double annual_cagr = 0.10, double annual_volatility = 0.15, uint32_t seed = 42)
        : rng(seed), normal_pool(1024), annual_volatility(annual_volatility)
    {
        log_mean = std::log(1.0 + annual_cagr) - 0.5 * (annual_volatility * annual_volatility);;
    }

    void set_seed(uint32_t seed) {
        rng.seed(seed);
        normal_pool.reset();
    }

    inline double generate_annual_multiplier() noexcept {
        double log_return = log_mean + annual_volatility * normal_pool.get_next(rng);

        // e^(r) guarantees that negative returns never drop portfolio below $0
        return std::exp(log_return);
    }

    inline double apply_annual_growth(double current_aum) noexcept {
        return current_aum * generate_annual_multiplier();
    }
};