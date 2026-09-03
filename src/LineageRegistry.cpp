#include "LineageRegistry.hpp"
#include <algorithm>
#include <cmath>

uint32_t LineageRegistry::create_root_branch(uint32_t branch_id) {
    branch_arena.clear();
    beneficiary_arena.clear();

    // emplace the BranchNode
    branch_arena.emplace_back(branch_id, Constant::HUNDRED_PERCENT, INVALID_INDEX);
    return Constant::ROOT_INDEX;
}


uint32_t LineageRegistry::add_sub_branch(uint32_t parent_index, uint32_t branch_id) {
    if(parent_index >= branch_arena.size()) return INVALID_INDEX;

    uint32_t child_index = static_cast<uint32_t>(branch_arena.size());

    // child BranchNode
    branch_arena.emplace_back(branch_id, Constant::ZERO_PERCENT, parent_index);

    BranchNode& parent = branch_arena[parent_index];
    if (parent.child_count == 0) {
        parent.first_child_index = child_index;
    } 
    else {
        // Find the last child in the sibling chain and link it to this new child
        uint32_t curr = parent.first_child_index;
        while (branch_arena[curr].next_sibling_index != INVALID_INDEX) {
            curr = branch_arena[curr].next_sibling_index;
        }
        branch_arena[curr].next_sibling_index = child_index;
    }

    parent.child_count++;

    rebalance_per_stirpes_shares();

    return child_index;
}

uint32_t LineageRegistry::add_beneficiary(uint32_t branch_index, 
                                           uint64_t beneficiary_id, 
                                           uint16_t age, 
                                           double annual_contribution) 
{
    if (branch_index >= branch_arena.size()) return INVALID_INDEX;

    uint32_t beneficiary_index = static_cast<uint32_t>(beneficiary_arena.size());

    HeirState initial_state = (age < 21) ? HeirState::MINOR : HeirState::INACTIVE;
    if (age >= 21 && annual_contribution > 0.0) {
        initial_state = HeirState::ACTIVE;
    }
    
    // Construct beneficiary
    Beneficiary heir(beneficiary_id, age, branch_arena[branch_index].branch_id);
    
    // Set direct arena index of the parent branch
    heir.branch_index = branch_index; 
    heir.state = initial_state;
    heir.annual_capital_contribution = annual_contribution;

    beneficiary_arena.push_back(heir);

    // Link beneficiary to branch node
    BranchNode& branch = branch_arena[branch_index];
    if (branch.active_heir_count == 0 && branch.heir_start_index == INVALID_INDEX) {
        branch.heir_start_index = beneficiary_index;
    }

    if (initial_state == HeirState::ACTIVE) {
        branch.active_heir_count++;
    }
    return beneficiary_index;
}

void LineageRegistry::rebalance_per_stirpes_shares() noexcept {
    if (branch_arena.empty()) return;
    
    // ROOT started 100%
    branch_arena[0].virtual_share_percentage = 1.0;

    for (size_t i = 0; i < branch_arena.size(); ++i) {
        const auto& parent = branch_arena[i];

        // if not count
        if (parent.child_count > 0 && parent.first_child_index != INVALID_INDEX) 
        {
            double split_share = parent.virtual_share_percentage / static_cast<double>(parent.child_count);
            uint32_t child_idx = parent.first_child_index;

            while (child_idx != INVALID_INDEX && child_idx < branch_arena.size()) 
            {
                branch_arena[child_idx].virtual_share_percentage = split_share;
                child_idx = branch_arena[child_idx].next_sibling_index;
            }
        }
    }

}

double LineageRegistry::execute_annual_settlement(double global_cap, const PolicyEngine& policy) noexcept{
    if(global_cap <= 0 || beneficiary_arena.empty()) {
        last_settled_payout = 0;
        return 0;
    }

    double total_disbursed {0.0};
    std::vector<double> branch_base_caps(branch_arena.size(), 0.0);
    std::vector<double> branch_disbursed(branch_arena.size(), 0.0);
    std::vector<double> heir_raw_claims(beneficiary_arena.size(), 0.0);

    // -------------------------------------------------------------------------
    // PHASE 1: BASE GUARANTEE SETTLEMENT
    // -------------------------------------------------------------------------

    // Calculate base caps per branch
    for (size_t i = 0; i < branch_arena.size(); ++i) {
        branch_base_caps[i] = global_cap * branch_arena[i].virtual_share_percentage;
    }

    // Process raw claims and fulfill guaranteed base limits
    for (size_t i = 0; i < beneficiary_arena.size(); ++i) {
        Beneficiary& heir = beneficiary_arena[i];

        // reset last_approve
        heir.last_approved_base_payout = 0.0;

        if (heir.state != HeirState::ACTIVE || heir.annual_capital_contribution <= 0.0) {
            continue;
        }
        double raw_match = heir.annual_capital_contribution * policy.get_net_multiplier();
        heir_raw_claims[i] = raw_match;
        
        // find the branch for the heir
        uint32_t branch_idx = INVALID_INDEX;
        for (size_t b = 0; b < branch_arena.size(); ++b) {
            if (branch_arena[b].branch_id == heir.branch_id) {
                branch_idx = static_cast<uint32_t>(b);
                break;
            }
        }

        if (branch_idx == INVALID_INDEX) continue;

        double heir_base_cap = branch_arena[branch_idx].calculate_individual_heir_cap(branch_base_caps[branch_idx]);

        // which one is smaller
        double base_payout = std::min(raw_match, heir_base_cap);
        heir.last_approved_base_payout = base_payout;

        branch_disbursed[branch_idx] += base_payout;
        total_disbursed += base_payout;
    }

    // -------------------------------------------------------------------------
    // PHASE 2 & 3: HYBRID SURPLUS SPILLOVER
    // -------------------------------------------------------------------------
    if (policy.get_mode() == GovernanceMode::HYBRID_SPILLOVER){
        double surplus_pool = global_cap - total_disbursed;

        if (surplus_pool > 0.0) {
            for (size_t i = 0; i < beneficiary_arena.size(); ++i) {
                Beneficiary& heir = beneficiary_arena[i];

                double remaining_demand = heir_raw_claims[i] - heir.last_approved_base_payout;
                if (remaining_demand <= 0.0) continue;

                // Locate branch node
                uint32_t branch_idx = INVALID_INDEX;
                for (size_t b = 0; b < branch_arena.size(); ++b) {
                    if (branch_arena[b].branch_id == heir.branch_id) {
                        branch_idx = static_cast<uint32_t>(b);
                        break;
                    }
                }
                if (branch_idx == INVALID_INDEX) continue;

                // Evaluate spillover room (1.5x Base Cap Ceiling)
                double branch_ceiling = policy.calculate_branch_ceiling(global_cap, branch_arena[branch_idx].virtual_share_percentage);
                double branch_spillover_room = branch_ceiling - branch_disbursed[branch_idx];


                if (branch_spillover_room > 0.0) {
                    double spillover_grant = std::min({remaining_demand, branch_spillover_room, surplus_pool});

                    heir.last_approved_spillover_payout += spillover_grant;
                    branch_disbursed[branch_idx] += spillover_grant;
                    surplus_pool -= spillover_grant;
                    total_disbursed += spillover_grant;

                    if (surplus_pool <= 0.0) break;
                }
            }
        }
    }

    last_settled_payout = total_disbursed;
    return total_disbursed;
}

double LineageRegistry::execute_annual_settlement_future(double global_cap, const PolicyEngine &policy) noexcept
{
    if (global_cap <= 0.0 || beneficiary_arena.empty()) {
        last_settled_payout = 0.0;
        return 0.0;
    }
    // std::fill(annual_branch_base_caps.begin(), annual_branch_base_caps.end(), 0.0);
    // std::fill(annual_branch_disbursed.begin(), annual_branch_disbursed.end(), 0.0);
    // std::fill(annual_heir_raw_claims.begin(), annual_heir_raw_claims.end(), 0.0);
    annual_branch_base_caps.assign(branch_arena.size(), 0.0);
    annual_branch_disbursed.assign(branch_arena.size(), 0.0);
    annual_heir_raw_claims.assign(beneficiary_arena.size(), 0.0);

    double total_disbursed {0.0};
    rebuild_active_heir_indice();

    // Precompute branch caps once per year, reusing the same scratch storage.
    for (size_t i = 0; i < branch_arena.size(); ++i) {
        annual_branch_base_caps[i] = global_cap * branch_arena[i].virtual_share_percentage;
    }

    // Phase 1: settle only active heirs using the cached branch lookup created at add_beneficiary.
    for (uint32_t heir_index : active_heir_index) {
        Beneficiary& heir = beneficiary_arena[heir_index];
        
        heir.last_approved_base_payout = 0.0;
        heir.last_approved_spillover_payout = 0.0;
        uint32_t branch_idx = heir.branch_index;

        double raw_match = heir.annual_capital_contribution * policy.get_net_multiplier();
        annual_heir_raw_claims[heir_index] = raw_match;

        double heir_base_cap = branch_arena[branch_idx].calculate_individual_heir_cap(annual_branch_base_caps[branch_idx]);
        double base_payout = std::min(raw_match, heir_base_cap);

        heir.last_approved_base_payout = base_payout;
        annual_branch_disbursed[branch_idx] += base_payout;
        total_disbursed += base_payout;
    }

    // Phase 2 & 3: only process active heirs again, using the cached branch index.
    if (policy.get_mode() == GovernanceMode::HYBRID_SPILLOVER) {
        double surplus_pool = global_cap - total_disbursed;

        if (surplus_pool > 0.0) {
            for (uint32_t heir_index : active_heir_index) {
                Beneficiary& heir = beneficiary_arena[heir_index];
                double remaining_demand = annual_heir_raw_claims[heir_index] - heir.last_approved_base_payout;
                if (remaining_demand <= 0.0) {
                    continue;
                }

                uint32_t branch_idx = heir.branch_index;

                double branch_ceiling = policy.calculate_branch_ceiling(global_cap, branch_arena[branch_idx].virtual_share_percentage);
                double branch_spillover_room = branch_ceiling - annual_branch_disbursed[branch_idx];

                if (branch_spillover_room > 0.0) {
                    double spillover_grant = std::min({remaining_demand, branch_spillover_room, surplus_pool});

                    heir.last_approved_spillover_payout += spillover_grant;
                    annual_branch_disbursed[branch_idx] += spillover_grant;
                    surplus_pool -= spillover_grant;
                    total_disbursed += spillover_grant;

                    if (surplus_pool <= 0.0) break;
                }
            }
        }
    }

    last_settled_payout = total_disbursed;
    return total_disbursed;
}

void LineageRegistry::capture_telemetry_snapshot(AnnualSnapshot &snap, double global_cap, const PolicyEngine &policy) const
{
    snap.branch_states.reserve(branch_arena.size());
    snap.heir_states.reserve(beneficiary_arena.size());

    double net_multiplier = policy.get_net_multiplier();

    for (size_t i = 0; i < beneficiary_arena.size(); ++i) {
        const Beneficiary& heir = beneficiary_arena[i];

        HeirSnapshot h_snap;
        h_snap.heir_id = heir.id;
        h_snap.branch_id = heir.branch_id;
        h_snap.age = heir.age;
        h_snap.capital_contribution = heir.annual_capital_contribution;
        
        // Zero magic numbers:
        h_snap.raw_match_demand = heir.annual_capital_contribution * net_multiplier;
        
        h_snap.base_payout = heir.last_approved_base_payout;
        h_snap.spillover_payout = heir.last_approved_spillover_payout;
        h_snap.unmet_demand = h_snap.raw_match_demand - h_snap.total_payout();
       

        snap.heir_states.push_back(h_snap);
    }
}

void LineageRegistry::reserve_capacity(size_t expected_branches, size_t expected_heirs) {
    branch_arena.reserve(expected_branches);
    beneficiary_arena.reserve(expected_heirs);
    annual_branch_base_caps.reserve(expected_branches);
    annual_branch_disbursed.reserve(expected_branches);
    annual_heir_raw_claims.reserve(expected_heirs);
}

// void LineageRegistry::sync_runtime_buffers() noexcept {
//     annual_branch_base_caps.resize(branch_arena.size(), 0.0);
//     annual_branch_disbursed.resize(branch_arena.size(), 0.0);
//     annual_heir_raw_claims.resize(beneficiary_arena.size(), 0.0);
// }

uint32_t LineageRegistry::get_branch_index_by_id(uint32_t branch_id) const {
    for (uint32_t idx = 0; idx < branch_arena.size(); ++idx) {
        if (branch_arena[idx].branch_id == branch_id) {
            return idx; // Returns 0..99
        }
    }
    return INVALID_INDEX;
}


void LineageRegistry::rebuild_active_heir_indice() noexcept{
    active_heir_index.clear();
    active_heir_index.reserve(beneficiary_arena.size());

    for(uint32_t i = 0; i < beneficiary_arena.size(); ++i) {
        const Beneficiary& heir = beneficiary_arena[i];
        if(heir.state == HeirState::ACTIVE && heir.annual_capital_contribution > 0.0) {
            active_heir_index.push_back(i);
        }
    }
}