#pragma once

#include <cstdint>
#include <vector>

struct HeirSnapshot {

    uint64_t heir_id{0};
    uint32_t branch_id{0};
    uint16_t age{0};

    double capital_contribution{0.0};
    double raw_match_demand{0.0};
    double base_payout{0.0};
    double spillover_payout{0.0};
    double unmet_demand{0.0};

    [[nodiscard]] inline double total_payout() const noexcept { 
        return base_payout + spillover_payout; 
    }

    [[nodiscard]] inline double effective_match_ratio() const noexcept {
        return (capital_contribution > 0.0) ? (total_payout() / capital_contribution) : 0.0;
    }
};

struct BranchSnapshot {
    uint32_t branch_id{0};
    uint32_t parent_index{0};
    double virtual_share_percentage{0.0};
    
    double base_cap_dollars{0.0};        // Phase 1 Guaranteed Ceiling
    double base_disbursed{0.0};          // Phase 1 Paid Out
    double spillover_disbursed{0.0};     // Phase 3 Surplus Paid Out
    uint16_t active_heir_count{0};
    
    [[nodiscard]] inline double total_disbursed() const noexcept { 
        return base_disbursed + spillover_disbursed; 
    }
};


struct AnnualSnapshot {
    uint32_t year{0};
    double starting_aum{0.0};
    double ending_aum{0.0};
    double annual_market_return{0.0};
    
    double global_cap_dollars{0.0};      // 3.0% of AUM
    double total_base_disbursed{0.0};    // Sum of all Phase 1 payouts
    double total_spillover_disbursed{0.0};// Sum of all Phase 3 payouts
    double unused_surplus_retained{0.0}; // Re-invested into S&P 500
    
    std::vector<BranchSnapshot> branch_states;
    std::vector<HeirSnapshot> heir_states;
};