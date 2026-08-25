#pragma once

#include <algorithm>
#include <cstdint>

enum class GovernanceMode {
    STRICT_PARTITION,
    DYNAMIC_POOLING,
    HYBRID_SPILLOVER
};


struct PolicyConfig {
    double net_match_multiplier     {3.0};
    double global_cap_ratio         {0.03};
    double spillover_cap_ratio      {1.50};
    uint16_t age_of_majority        {21};
    GovernanceMode mode             {GovernanceMode::HYBRID_SPILLOVER};
};



class PolicyEngine {
private:
    PolicyConfig config;

public:
    explicit PolicyEngine(PolicyConfig cfg = PolicyConfig{}): config(cfg) {}

    // getter
    inline GovernanceMode get_mode() const noexcept { return config.mode; }
    inline double get_net_multiplier() const noexcept { return config.net_match_multiplier; }
    inline double get_global_cap_ratio() const noexcept { return config.global_cap_ratio; }

    inline double calculate_global_cap(double total_aum) const noexcept {
        return total_aum * config.global_cap_ratio;
    }

    /*
    [ Master Fund AUM = $10,000,000 ]
    └─ global_cap = $300,000 (3% limit)
       │
       ├─► Branch A (branch_share = 0.50)
       │    └─ base_cap = $300,000 * 0.50 = $150,000
       │
       └─► Branch B (branch_share = 0.50)
            └─ base_cap = $300,000 * 0.50 = $150,000
    */
    inline double calculate_branch_ceiling(double global_cap, double branch_share) const noexcept {
        double base_cap = global_cap * branch_share;

        switch (config.mode) {
            case GovernanceMode::STRICT_PARTITION:
                return base_cap; // Hard ceiling at base entitlement

            case GovernanceMode::HYBRID_SPILLOVER:
                // Base entitlement + capped spillover allowance
                return base_cap * config.spillover_cap_ratio;

            case GovernanceMode::DYNAMIC_POOLING:
                return global_cap; // Can theoretically claim up to full global limit
        }
        return base_cap;
    }

};
