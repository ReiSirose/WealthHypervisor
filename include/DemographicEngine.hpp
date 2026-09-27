#pragma once

#include "MasterFund.hpp"
#include "MarketEngine.hpp"
#include "random.h"
#include <random>
#include <vector>
#include <cstdint>

struct DemographicConfig {
    size_t initial_branches        {100};
    size_t initial_heirs           {250};
    double annual_birth_rate       {0.03};
    double annual_branch_rate      {0.05};
    double contribution_volatility {0.20};
    uint32_t seed                  {1337};
};

class DemographicEngine {
public:
    explicit DemographicEngine(DemographicConfig config = {});

    // Seeds initial branch network and beneficiary arena via public APIs
    void seed_estate(MasterFund& fund);

    // Advances demographic lifecycle (ages beneficiaries, adds children, creates sub-branches)
    void step_demographics(MasterFund& fund);

private:
    DemographicConfig config_;
    Xoshiro256PlusPlus rng;
    std::uniform_real_distribution<double> probability_dist{0.0, 1.0};
    NormalRandomPool contribution_shock_pool;
    std::vector<double> base_contributions;
};