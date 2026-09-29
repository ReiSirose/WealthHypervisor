#include <gtest/gtest.h>
#include "LineageRegistry.hpp"
#include "BranchNode.hpp"

// 1. BranchNode Direct Unit Tests

TEST(BranchNodeTest, NodePropertiesAndCapCalculation) {
    // Test root node initialization
    BranchNode root(100, 1.0, INVALID_INDEX);
    EXPECT_TRUE(root.is_root());
    EXPECT_TRUE(root.is_leaf());
    EXPECT_FALSE(root.has_heirs());

    // Test non-root leaf node
    BranchNode leaf(200, 0.5, 0);
    EXPECT_FALSE(leaf.is_root());
    EXPECT_TRUE(leaf.is_leaf());

    // Test heir cap division with active heirs
    leaf.active_heir_count = 4;
    leaf.heir_start_index = 0;
    EXPECT_TRUE(leaf.has_heirs());
    EXPECT_DOUBLE_EQ(leaf.calculate_individual_heir_cap(10'000.0), 2'500.0);

    // Test heir cap division with zero active heirs (prevents divide-by-zero)
    BranchNode empty_node(300, 0.5, 0);
    EXPECT_DOUBLE_EQ(empty_node.calculate_individual_heir_cap(10'000.0), 0.0);
}

// 2. LineageRegistry Tree & Per-Stirpes Tests

class LineageRegistryTest : public ::testing::Test {
protected:
    LineageRegistry registry;
};

TEST_F(LineageRegistryTest, CreateRootBranchResetsAndInitializes) {
    uint32_t root_idx = registry.create_root_branch(100);

    EXPECT_EQ(root_idx, Constant::ROOT_INDEX);
    EXPECT_EQ(registry.branch_count(), 1);

    const auto& branches = registry.get_branches();
    EXPECT_EQ(branches[0].branch_id, 100);
    EXPECT_DOUBLE_EQ(branches[0].virtual_share_percentage, Constant::HUNDRED_PERCENT);
    EXPECT_TRUE(branches[0].is_root());
}

TEST_F(LineageRegistryTest, AddSubBranchesAndVerifySiblingPointers) {
    uint32_t root = registry.create_root_branch(100);

    uint32_t child_a = registry.add_sub_branch(root, 200);
    uint32_t child_b = registry.add_sub_branch(root, 300);
    uint32_t child_c = registry.add_sub_branch(root, 400);

    EXPECT_EQ(registry.branch_count(), 4);

    const auto& branches = registry.get_branches();
    
    // Validate parent properties
    EXPECT_EQ(branches[root].child_count, 3);
    EXPECT_EQ(branches[root].first_child_index, child_a);

    // Validate sibling linked-list chain: A -> B -> C -> INVALID_INDEX
    EXPECT_EQ(branches[child_a].next_sibling_index, child_b);
    EXPECT_EQ(branches[child_b].next_sibling_index, child_c);
    EXPECT_EQ(branches[child_c].next_sibling_index, INVALID_INDEX);
}

TEST_F(LineageRegistryTest, RebalancePerStirpesSharesTopDown) {
    // Construct a multi-tier tree:
    // Root (100%)
    //  ├── Branch A (50%)
    //  │    ├── Branch A1 (25%)
    //  │    └── Branch A2 (25%)
    //  └── Branch B (50%)

    uint32_t root = registry.create_root_branch(100);

    uint32_t branch_a = registry.add_sub_branch(root, 200);
    uint32_t branch_b = registry.add_sub_branch(root, 300);

    uint32_t branch_a1 = registry.add_sub_branch(branch_a, 201);
    uint32_t branch_a2 = registry.add_sub_branch(branch_a, 202);

    const auto& branches = registry.get_branches();

    EXPECT_DOUBLE_EQ(branches[root].virtual_share_percentage, 1.0);
    EXPECT_DOUBLE_EQ(branches[branch_a].virtual_share_percentage, 0.5);
    EXPECT_DOUBLE_EQ(branches[branch_b].virtual_share_percentage, 0.5);
    EXPECT_DOUBLE_EQ(branches[branch_a1].virtual_share_percentage, 0.25);
    EXPECT_DOUBLE_EQ(branches[branch_a2].virtual_share_percentage, 0.25);
}

// 3. Beneficiary Management & Heir State Tests
TEST_F(LineageRegistryTest, AddBeneficiariesAndAssignHeirStates) {
    uint32_t root = registry.create_root_branch(100);
    uint32_t branch_a = registry.add_sub_branch(root, 200);

    // 1. Minor Heir (< 21 years old)
    uint32_t minor_idx = registry.add_beneficiary(branch_a, 1001, 16, 5'000.0);
    
    // 2. Adult with zero contribution is still active
    uint32_t zero_contribution_idx = registry.add_beneficiary(branch_a, 1002, 25, 0.0);

    // 3. Adult Active Heir (>= 21 years old, > 0 contribution)
    uint32_t active_idx = registry.add_beneficiary(branch_a, 1003, 30, 10'000.0);

    EXPECT_EQ(registry.beneficiary_count(), 3);

    const auto& beneficiaries = registry.get_beneficiaries();
    const auto& branches = registry.get_branches();

    // Verify Heir State Assignments
    EXPECT_EQ(beneficiaries[minor_idx].state, HeirState::MINOR);
    EXPECT_EQ(beneficiaries[zero_contribution_idx].state, HeirState::ACTIVE);
    EXPECT_EQ(beneficiaries[active_idx].state, HeirState::ACTIVE);

    // Verify Branch Heir Counters and Linkage
    EXPECT_EQ(branches[branch_a].heir_start_index, minor_idx); // First heir added to branch
    EXPECT_EQ(branches[branch_a].active_heir_count, 2);         // Both adults are active
}


// 4. Edge Cases & Out-of-Bounds Protections
TEST_F(LineageRegistryTest, HandleInvalidIndicesGracefully) {
    registry.create_root_branch(100);

    // Invalid Parent Index for Sub-Branch
    uint32_t invalid_branch = registry.add_sub_branch(999, 200);
    EXPECT_EQ(invalid_branch, INVALID_INDEX);

    // Invalid Branch Index for Beneficiary
    uint32_t invalid_heir = registry.add_beneficiary(999, 1001, 25, 1'000.0);
    EXPECT_EQ(invalid_heir, INVALID_INDEX);
}


class PerStirpesRebalanceTest : public ::testing::Test {
protected:
    LineageRegistry registry;

    // Helper to calculate the sum of virtual shares across all leaf nodes
    double calculate_leaf_share_sum() const {
        double sum = 0.0;
        for (const auto& branch : registry.get_branches()) {
            if (branch.is_leaf()) {
                sum += branch.virtual_share_percentage;
            }
        }
        return sum;
    }
};
// 1. Edge Cases
TEST_F(PerStirpesRebalanceTest, EmptyRegistryDoesNotCrash) {
    // Calling rebalance on an empty registry should safely return without faulting
    EXPECT_NO_THROW(registry.rebalance_per_stirpes_shares());
    EXPECT_EQ(registry.branch_count(), 0);
}

TEST_F(PerStirpesRebalanceTest, SingleRootRetainsHundredPercent) {
    registry.create_root_branch(100);
    
    // Explicitly mutate root share to verify rebalance forces it back to 1.0
    auto& branches = const_cast<std::vector<BranchNode>&>(registry.get_branches());
    branches[0].virtual_share_percentage = 0.42;

    registry.rebalance_per_stirpes_shares();

    EXPECT_DOUBLE_EQ(branches[0].virtual_share_percentage, 1.0);
    EXPECT_DOUBLE_EQ(calculate_leaf_share_sum(), 1.0);
}

// 2. Single-Tier Splits (Root -> N Direct Children)
TEST_F(PerStirpesRebalanceTest, EvenSplitAcrossDirectChildren) {
    uint32_t root = registry.create_root_branch(100);

    // Root -> 4 Children (Should each get 25%)
    registry.add_sub_branch(root, 201);
    registry.add_sub_branch(root, 202);
    registry.add_sub_branch(root, 203);
    registry.add_sub_branch(root, 204);

    registry.rebalance_per_stirpes_shares();

    const auto& branches = registry.get_branches();
    EXPECT_DOUBLE_EQ(branches[0].virtual_share_percentage, 1.0); // Root

    for (size_t i = 1; i <= 4; ++i) {
        EXPECT_DOUBLE_EQ(branches[i].virtual_share_percentage, 0.25);
    }

    EXPECT_DOUBLE_EQ(calculate_leaf_share_sum(), 1.0);
}

TEST_F(PerStirpesRebalanceTest, UnevenFloatingPointDivision) {
    uint32_t root = registry.create_root_branch(100);

    // Root -> 3 Children (1.0 / 3 = 0.3333333...)
    registry.add_sub_branch(root, 201);
    registry.add_sub_branch(root, 202);
    registry.add_sub_branch(root, 203);

    registry.rebalance_per_stirpes_shares();

    const auto& branches = registry.get_branches();
    double expected_share = 1.0 / 3.0;

    for (size_t i = 1; i <= 3; ++i) {
        EXPECT_NEAR(branches[i].virtual_share_percentage, expected_share, 1e-9);
    }

    // Leaf shares must still sum to 1.0
    EXPECT_NEAR(calculate_leaf_share_sum(), 1.0, 1e-9);
}

// 3. Multi-Tier & Asymmetric Tree Propagation
TEST_F(PerStirpesRebalanceTest, AsymmetricMultiTierRebalance) {
    /* Tree Topology:
     * Root (1.0)
     *  ├── Branch A (0.50)
     *  │    ├── Branch A1 (0.25)
     *  │    └── Branch A2 (0.25)
     *  └── Branch B (0.50)
     *       ├── Branch B1 (0.16666...)
     *       ├── Branch B2 (0.16666...)
     *       └── Branch B3 (0.16666...)
     */

    uint32_t root = registry.create_root_branch(100);

    uint32_t branch_a = registry.add_sub_branch(root, 200);
    uint32_t branch_b = registry.add_sub_branch(root, 300);

    // Sub-branches of A (2 children)
    uint32_t branch_a1 = registry.add_sub_branch(branch_a, 201);
    uint32_t branch_a2 = registry.add_sub_branch(branch_a, 202);

    // Sub-branches of B (3 children)
    uint32_t branch_b1 = registry.add_sub_branch(branch_b, 301);
    uint32_t branch_b2 = registry.add_sub_branch(branch_b, 302);
    uint32_t branch_b3 = registry.add_sub_branch(branch_b, 303);

    registry.rebalance_per_stirpes_shares();

    const auto& branches = registry.get_branches();

    // Level 1 checks
    EXPECT_DOUBLE_EQ(branches[branch_a].virtual_share_percentage, 0.5);
    EXPECT_DOUBLE_EQ(branches[branch_b].virtual_share_percentage, 0.5);

    // Level 2 checks (Branch A sub-tree)
    EXPECT_DOUBLE_EQ(branches[branch_a1].virtual_share_percentage, 0.25);
    EXPECT_DOUBLE_EQ(branches[branch_a2].virtual_share_percentage, 0.25);

    // Level 2 checks (Branch B sub-tree)
    double expected_b_child_share = 0.5 / 3.0; // 0.16666...
    EXPECT_NEAR(branches[branch_b1].virtual_share_percentage, expected_b_child_share, 1e-9);
    EXPECT_NEAR(branches[branch_b2].virtual_share_percentage, expected_b_child_share, 1e-9);
    EXPECT_NEAR(branches[branch_b3].virtual_share_percentage, expected_b_child_share, 1e-9);

    // Total active leaf pool sum must equal 100%
    EXPECT_NEAR(calculate_leaf_share_sum(), 1.0, 1e-9);
}

// 4. Dynamic Tree Mutation & Recalculation

TEST_F(PerStirpesRebalanceTest, RecalculatesCorrectlyOnDynamicBranchAddition) {
    uint32_t root = registry.create_root_branch(100);

    // Step 1: Add first child -> gets 100%
    uint32_t child_1 = registry.add_sub_branch(root, 201);
    EXPECT_DOUBLE_EQ(registry.get_branches()[child_1].virtual_share_percentage, 1.0);

    // Step 2: Add second child -> rebalances to 50% / 50%
    uint32_t child_2 = registry.add_sub_branch(root, 202);
    EXPECT_DOUBLE_EQ(registry.get_branches()[child_1].virtual_share_percentage, 0.5);
    EXPECT_DOUBLE_EQ(registry.get_branches()[child_2].virtual_share_percentage, 0.5);

    // Step 3: Add third child -> rebalances to 33.33% each
    uint32_t child_3 = registry.add_sub_branch(root, 203);
    EXPECT_NEAR(registry.get_branches()[child_1].virtual_share_percentage, 1.0 / 3.0, 1e-9);
    EXPECT_NEAR(registry.get_branches()[child_2].virtual_share_percentage, 1.0 / 3.0, 1e-9);
    EXPECT_NEAR(registry.get_branches()[child_3].virtual_share_percentage, 1.0 / 3.0, 1e-9);

    EXPECT_NEAR(calculate_leaf_share_sum(), 1.0, 1e-9);
}

class ActiveHeirIndexTest : public ::testing::Test {
protected:
    LineageRegistry registry;
    uint32_t root_branch_idx{0};

    void SetUp() override {
        root_branch_idx = registry.create_root_branch(100);
    }
};

// 1. Edge Cases
TEST_F(ActiveHeirIndexTest, EmptyArenaProducesEmptyIndex) {
    registry.rebuild_active_heir_indice();
    
    EXPECT_TRUE(registry.get_active_heir_indices().empty());
    EXPECT_EQ(registry.get_active_heir_indices().size(), 0);
}

// 2. Filtering Truth Table Logic
TEST_F(ActiveHeirIndexTest, IncludesAllAdultsRegardlessOfContribution) {
    // Arena Index 0: Adult with valid contribution -> included
    registry.add_beneficiary(root_branch_idx, 1001, 25, 5'000.0);

    // Arena Index 1: Minor with contribution -> EXCLUDED (state != ACTIVE)
    registry.add_beneficiary(root_branch_idx, 1002, 16, 5'000.0);

    // Arena Index 2: Adult with 0 contribution -> included
    registry.add_beneficiary(root_branch_idx, 1003, 30, 0.0);

    // Arena Index 3: Adult with valid contribution -> included
    registry.add_beneficiary(root_branch_idx, 1004, 40, 10'000.0);

    // Rebuild active index table
    registry.rebuild_active_heir_indice();

    const auto& active_indices = registry.get_active_heir_indices();

    // Verify exact size and contents
    ASSERT_EQ(active_indices.size(), 3);
    EXPECT_EQ(active_indices[0], 0); // Heir 1001
    EXPECT_EQ(active_indices[1], 2); // Heir 1003
    EXPECT_EQ(active_indices[2], 3); // Heir 1004
}

TEST_F(ActiveHeirIndexTest, IncludesActiveHeirsWithZeroOrNegativeContribution) {
    // Contribution does not determine index membership.
    uint32_t idx = registry.add_beneficiary(root_branch_idx, 1001, 25, 1'000.0);
    
    // Manually force state to ACTIVE but clear contribution
    auto& heirs = registry.get_beneficiaries_mut();
    heirs[idx].annual_capital_contribution = 0.0;

    registry.rebuild_active_heir_indice();

    ASSERT_EQ(registry.get_active_heir_indices().size(), 1);
    EXPECT_EQ(registry.get_active_heir_indices()[0], idx);
}

// 3. Dynamic Mutations & Idempotency
TEST_F(ActiveHeirIndexTest, UpdatesCorrectlyWhenHeirStateChanges) {
    // Add a minor (starts non-active)
    uint32_t minor_idx = registry.add_beneficiary(root_branch_idx, 1001, 20, 2'500.0);

    registry.rebuild_active_heir_indice();
    EXPECT_EQ(registry.get_active_heir_indices().size(), 0);

    // Aging up transitions the beneficiary to ACTIVE and invalidates the cache.
    auto& heirs = registry.get_beneficiaries_mut();
    EXPECT_TRUE(heirs[minor_idx].tick_annual_aging());
    registry.mark_active_heir_index_dirty();

    // Rebuild and verify inclusion
    registry.rebuild_active_heir_indice();
    
    ASSERT_EQ(registry.get_active_heir_indices().size(), 1);
    EXPECT_EQ(registry.get_active_heir_indices()[0], minor_idx);
}

TEST_F(ActiveHeirIndexTest, IdempotentMultipleCallsDoNotDuplicateIndices) {
    registry.add_beneficiary(root_branch_idx, 1001, 25, 5'000.0);
    registry.add_beneficiary(root_branch_idx, 1002, 30, 2'000.0);

    // Call multiple times sequentially
    registry.rebuild_active_heir_indice();
    registry.rebuild_active_heir_indice();
    registry.rebuild_active_heir_indice();

    const auto& active_indices = registry.get_active_heir_indices();

    // Verify .clear() prevented buffer duplication
    ASSERT_EQ(active_indices.size(), 2);
    EXPECT_EQ(active_indices[0], 0);
    EXPECT_EQ(active_indices[1], 1);
}