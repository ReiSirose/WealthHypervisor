#pragma once

#include <cstdint>
#include <memory>

constexpr uint32_t INVALID_INDEX_BRANCH = std::numeric_limits<uint32_t>::max();

enum class HeirState : uint8_t {

    MINOR    = 0,   // Age < 21
    ACTIVE   = 2,   // Age >= 21; contribution determines payout amount
    DECEASED = 3,   // Terminal state -> Triggers rebalance
};

struct alignas(64) Beneficiary {
    uint64_t id;
    double annual_capital_contribution;
    double last_approved_base_payout;
    double last_approved_spillover_payout;
    uint32_t branch_id;
    uint32_t branch_index;
    uint16_t age;
    HeirState state;

    Beneficiary(uint64_t heir_id, uint16_t start_age, uint32_t parent_branch_id)
    : id(heir_id),
      annual_capital_contribution(0.0),
      last_approved_base_payout(0.0),
      last_approved_spillover_payout(0.0),
      branch_id(parent_branch_id),
      branch_index(INVALID_INDEX_BRANCH),
      age(start_age),
    state(start_age >= 21 ? HeirState::ACTIVE : HeirState::MINOR)
    {}

    inline bool is_eligible() const noexcept {
        return state == HeirState::ACTIVE;
    }

    inline bool tick_annual_aging() noexcept {
        if (state == HeirState::DECEASED) return false;

        ++age;

        if (state == HeirState::MINOR && age >= 21) {
            state = HeirState::ACTIVE;
            return true;
        }
        return false;
    }

    inline void deposit_capital(double cash_amount) noexcept {
        if (state == HeirState::DECEASED || state == HeirState::MINOR) return;

        annual_capital_contribution = cash_amount;
    }

    inline double calculate_raw_net_demand(double multiplier) const noexcept {
        if (state != HeirState::ACTIVE) return 0.0;
        
        // Multiplier of 3.0 means $1 contribution yields $2 bonus match from fund
        double net_multiplier = (multiplier > 1.0) ? (multiplier - 1.0) : 0.0;
        return annual_capital_contribution * net_multiplier;
    }
};
