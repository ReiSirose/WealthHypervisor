#pragma once
#include "BranchNode.hpp"
#include "Beneficiary.hpp"
#include "PolicyEngine.hpp"
#include "TelemetryTypes.hpp"

#include <vector>
#include <cstdint>
#include <cstddef>

namespace Constant {
    constexpr double    HUNDRED_PERCENT      {1.0};
    constexpr double    ZERO_PERCENT         {0.0};
    constexpr uint32_t  ROOT_INDEX           {0};
};

class LineageRegistry {
private:
    std::vector<BranchNode>  branch_arena;
    std::vector<Beneficiary> beneficiary_arena;

    double last_settled_payout{0.0};

public:
    uint32_t create_root_branch(uint32_t branch_id);
    
    uint32_t add_sub_branch(uint32_t parent_index, uint32_t branch_id);

    uint32_t add_beneficiary(uint32_t branch_index, 
                            uint64_t beneficiary_id, 
                            uint16_t age, 
                            double annual_contribution);

    // Traverses the flat arena top-down and rebalances percentage shares per-stirpes
    void rebalance_per_stirpes_shares() noexcept;

    uint32_t get_branch_index_by_id(uint32_t branch_id) const;
    // Executes 3-Phase Hybrid Settlement across all branches and heirs
    // Returns total cash disbursed from the fund in this cycle
    double execute_annual_settlement(double global_cap, const PolicyEngine& policy) noexcept;

    void capture_telemetry_snapshot(AnnualSnapshot& snap,  double global_cap, const PolicyEngine& policy) const;

    void reserve_capacity(size_t expected_branches, size_t expected_heirs);
    //getter 
    [[nodiscard]] inline size_t branch_count() const noexcept { return branch_arena.size(); }
    [[nodiscard]] inline size_t beneficiary_count() const noexcept { return beneficiary_arena.size(); }
    [[nodiscard]] inline double get_last_settled_payout() const noexcept { return last_settled_payout; }

    [[nodiscard]] inline const std::vector<BranchNode>& get_branches() const noexcept { return branch_arena; }
    [[nodiscard]] inline const std::vector<Beneficiary>& get_beneficiaries() const noexcept { return beneficiary_arena; }
    [[nodiscard]] inline std::vector<Beneficiary>& get_beneficiaries_mut() noexcept { return beneficiary_arena; }
};