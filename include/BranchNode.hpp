#pragma once
#include <cstdint>
#include <limits>

constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

struct alignas(32) BranchNode {
    uint32_t branch_id            {0};             //  4 Bytes: Unique branch identifier
    uint32_t parent_index         {INVALID_INDEX}; //  4 Bytes: Parent index in global arena (INVALID_INDEX = Root)
    
    double   virtual_share_percentage{0.0};        //  8 Bytes: Per-stirpes share ratio (0.50 = 50%)
    
    uint32_t first_child_index    {INVALID_INDEX}; //  4 Bytes: Arena index where contiguous children start
    uint32_t next_sibling_index   {INVALID_INDEX};
    uint16_t child_count          {0};             //  2 Bytes: Number of direct sub-branches
    uint16_t active_heir_count    {0};             //  2 Bytes: Count of active contributing heirs
    
    uint32_t heir_start_index     {INVALID_INDEX}; //  4 Bytes: Start index in global std::vector<Beneficiary>

    BranchNode() = default;

    BranchNode(uint32_t id, double share, uint32_t parent_idx = INVALID_INDEX)
        : branch_id(id), 
          parent_index(parent_idx), 
          virtual_share_percentage(share) {}

    [[nodiscard]] inline bool is_root() const noexcept {
        return parent_index == INVALID_INDEX;
    }

    [[nodiscard]] inline bool is_leaf() const noexcept {
        return child_count == 0;
    }

    [[nodiscard]] inline bool has_heirs() const noexcept {
        return active_heir_count > 0 && heir_start_index != INVALID_INDEX;
    }

    [[nodiscard]] inline double calculate_individual_heir_cap(double branch_cap) const noexcept {
        if (active_heir_count == 0) return 0.0;
        return branch_cap / static_cast<double>(active_heir_count);
    }

};



