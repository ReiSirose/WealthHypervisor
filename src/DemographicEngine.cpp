#include "DemographicEngine.hpp"
#include <algorithm>
#include <iostream>

DemographicEngine::DemographicEngine(DemographicConfig config) : config_(config), rng(config.seed)
{

}

void DemographicEngine::seed_estate(MasterFund& fund) {

    if (config_.initial_branches == 0) return;

    fund.get_lineages_mut().reserve_capacity(config_.initial_branches * 2, config_.initial_heirs * 4);

    fund.create_root_branch(100);
    

    for (size_t branch_num = 1; branch_num < config_.initial_branches; ++branch_num) {
        std::uniform_int_distribution<size_t> parent_dist(0, static_cast<size_t>(branch_num - 1));
        uint32_t parent_idx = parent_dist(rng);

        uint32_t new_branch_code = static_cast<uint32_t>(100 + branch_num);
        fund.add_sub_branch(parent_idx, new_branch_code);
    }

    // Populate Initial Beneficiaries across [0, initial_branches - 1]
    std::uniform_int_distribution<uint32_t> branch_dist(0, static_cast<uint32_t>(config_.initial_branches - 1));
    std::uniform_int_distribution<uint16_t> age_dist(18, 60);
    std::uniform_real_distribution<double> contrib_dist(5'000.0, 50'000.0);

    for (size_t h = 0; h < config_.initial_heirs; ++h) {

        uint32_t assigned_branch_idx = branch_dist(rng);
        uint64_t heir_id = 2000 + h;

        uint16_t age = age_dist(rng);
        double base_contrib = contrib_dist(rng);

        fund.add_beneficiary(assigned_branch_idx, heir_id, age, base_contrib);
        base_contributions[heir_id] = base_contrib;
    }
}


void DemographicEngine::step_demographics(MasterFund& fund) {
    auto& lineages = fund.get_lineages_mut();
    
    // 1. Freeze initial count so newly born children are not iterated in the current year
    const size_t initial_heir_count = lineages.beneficiary_count();

    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    std::normal_distribution<double> shock_dist(1.0, config_.contribution_volatility);

    for (size_t i = 0; i < initial_heir_count; ++i) {
        // Direct index access keeps references safe if std::vector reallocates
        if (lineages.get_beneficiaries_mut()[i].state == HeirState::DECEASED) continue;

        lineages.get_beneficiaries_mut()[i].tick_annual_aging();

        // Safely capture fields for this iteration
        const uint16_t age = lineages.get_beneficiaries_mut()[i].age;
        const uint64_t heir_id = lineages.get_beneficiaries_mut()[i].id;
        
        // O(1) Arena Vector Index Read
        const uint32_t current_branch_idx = lineages.get_beneficiaries_mut()[i].branch_index;

        // --- Step A: Capital Contribution Deposit (Ages 21+) ---
        if (age >= 21) {
            double base_contrib = base_contributions[heir_id];
            if (base_contrib <= 0.0) {
                base_contrib = 10'000.0;
                base_contributions[heir_id] = base_contrib;
            }

            double shock = std::max(0.0, shock_dist(rng));
            lineages.get_beneficiaries_mut()[i].deposit_capital(base_contrib * shock);
        }

        // --- Step B: Dynamic Sub-Branch Spawning (Ages 25-35) ---
        if (age >= 25 && age <= 35) {
            if (prob_dist(rng) < config_.annual_branch_rate) {
                if (current_branch_idx < lineages.branch_count()) {
                    uint32_t new_branch_code = 10000 + static_cast<uint32_t>(lineages.branch_count());
                    
                    // Creates sub-branch in O(1) time
                    uint32_t new_branch_idx = fund.add_sub_branch(current_branch_idx, new_branch_code);
                    
                    // Update beneficiary's branch index and code
                    lineages.get_beneficiaries_mut()[i].branch_index = new_branch_idx;
                    lineages.get_beneficiaries_mut()[i].branch_id = new_branch_code;
                }
            }
        }

        // --- Step C: Dynamic Birth Spawning (Ages 22-40) ---
        if (age >= 22 && age <= 40) {
            if (prob_dist(rng) < config_.annual_birth_rate) {
                uint64_t child_id = 500000 + lineages.beneficiary_count();
                double child_base = base_contributions[heir_id] * 0.5;
                if (child_base <= 0.0) child_base = 10'000.0;

                // Re-fetch parent's branch index in case it was updated in Step B
                uint32_t parent_branch_idx = lineages.get_beneficiaries_mut()[i].branch_index;
                
                if (parent_branch_idx < lineages.branch_count()) {
                    // Spawns new child in O(1) time
                    fund.add_beneficiary(parent_branch_idx, child_id, 0, 0.0);
                    base_contributions[child_id] = child_base;
                }
            }
        }
    }
}