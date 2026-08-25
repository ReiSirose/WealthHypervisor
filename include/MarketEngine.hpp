#pragma once

#include <random>
#include <cmath>
#include <cstdint>


class MarketEngine {
private:  
    double target_cagr;
    double std_dev;
    double log_mean;

    std::mt19937 rng;
    std::normal_distribution<double> dist;

public:
    explicit MarketEngine(double annual_cagr = 0.10, double annual_volatility = 0.15, uint32_t seed = 42): 
                    target_cagr(annual_cagr), std_dev(annual_volatility), rng(seed)
    {
        log_mean = std::log(1.0 + annual_cagr) - 0.5 * (annual_volatility * annual_volatility);;

        dist = std::normal_distribution<double>(log_mean, annual_volatility);
    }

    void set_seed(uint32_t seed) {
        rng.seed(seed);
        dist.reset();
    }

    inline double generate_annual_multiplier() noexcept {
        double log_return = dist(rng);

        // e^(r) guarantees that negative returns never drop portfolio below $0
        return std::exp(log_return);
    }

    inline double apply_annual_growth(double current_aum) noexcept {
        return current_aum * generate_annual_multiplier();
    }
};