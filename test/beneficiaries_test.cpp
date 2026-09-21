#include <gtest/gtest.h>
#include<cstdint>
#include<limits>
#include <Beneficiary.hpp>

static constexpr uint32_t INVALID_INDEX_TEST = std::numeric_limits<uint32_t>::max();

TEST(BeneficiaryTest, MemoryAlignmentAndSize) {
    static_assert(alignof(Beneficiary) == 64, "Beneficiary must be 64-byte aligned");
    EXPECT_EQ(sizeof(Beneficiary), 64);
}

TEST(BeneficiaryTest, ConstructionAsMinor) {
    Beneficiary heir(1001, 18, 5);

    EXPECT_EQ(heir.id, 1001);
    EXPECT_EQ(heir.age, 18);
    EXPECT_EQ(heir.branch_id, 5);
    EXPECT_EQ(heir.branch_index, INVALID_INDEX_TEST);
    EXPECT_EQ(heir.state, HeirState::MINOR);
    EXPECT_FALSE(heir.is_eligible());
    EXPECT_DOUBLE_EQ(heir.annual_capital_contribution, 0.0);
    EXPECT_DOUBLE_EQ(heir.last_approved_base_payout, 0.0);
    EXPECT_DOUBLE_EQ(heir.last_approved_spillover_payout, 0.0);
}

TEST(BeneficiaryTest, ConstructionAsAdult) {
    Beneficiary heir(1002, 21, 5);

    EXPECT_EQ(heir.age, 21);
    EXPECT_EQ(heir.state, HeirState::INACTIVE);
    EXPECT_FALSE(heir.is_eligible());
}

// 2. Annual Aging Logic Tests

TEST(BeneficiaryTest, AgeIncrementForMinor) {
    Beneficiary heir(1001, 19, 1);
    
    heir.tick_annual_aging();
    EXPECT_EQ(heir.age, 20);
    EXPECT_EQ(heir.state, HeirState::MINOR);

    heir.tick_annual_aging();
    EXPECT_EQ(heir.age, 21);
    EXPECT_EQ(heir.state, HeirState::INACTIVE); // Transitions at 21
}

TEST(BeneficiaryTest, DeceasedHeirDoesNotAge) {
    Beneficiary heir(1001, 30, 1);
    heir.state = HeirState::DECEASED;

    heir.tick_annual_aging();
    EXPECT_EQ(heir.age, 30);
    EXPECT_EQ(heir.state, HeirState::DECEASED);
}

// 3. Capital Deposit & Eligibility State Machine Tests

TEST(BeneficiaryTest, MinorCannotDepositCapital) {
    Beneficiary heir(1001, 17, 1);

    heir.deposit_capital(10'000.0);
    EXPECT_DOUBLE_EQ(heir.annual_capital_contribution, 0.0);
    EXPECT_EQ(heir.state, HeirState::MINOR);
    EXPECT_FALSE(heir.is_eligible());
}


TEST(BeneficiaryTest, DeceasedCannotDepositCapital) {
    Beneficiary heir(1001, 25, 1);
    heir.state = HeirState::DECEASED;

    heir.deposit_capital(10'000.0);
    EXPECT_DOUBLE_EQ(heir.annual_capital_contribution, 0.0);
    EXPECT_EQ(heir.state, HeirState::DECEASED);
}

TEST(BeneficiaryTest, AdultValidDepositActivatesEligibility) {
    Beneficiary heir(1001, 25, 1);

    heir.deposit_capital(5'000.0);
    EXPECT_DOUBLE_EQ(heir.annual_capital_contribution, 5'000.0);
    EXPECT_EQ(heir.state, HeirState::ACTIVE);
    EXPECT_TRUE(heir.is_eligible());
}

TEST(BeneficiaryTest, ZeroOrNegativeDepositDeactivatesEligibility) {
    Beneficiary heir(1001, 25, 1);

    // Deposit positive capital -> ACTIVE
    heir.deposit_capital(5'000.0);
    EXPECT_TRUE(heir.is_eligible());

    // Deposit 0.0 capital -> INACTIVE
    heir.deposit_capital(0.0);
    EXPECT_DOUBLE_EQ(heir.annual_capital_contribution, 0.0);
    EXPECT_EQ(heir.state, HeirState::INACTIVE);
    EXPECT_FALSE(heir.is_eligible());
}

// 4. Net Demand Calculation Tests

TEST(BeneficiaryTest, NonActiveStateYieldsZeroDemand) {
    Beneficiary heir(1001, 25, 1); // Starts INACTIVE

    EXPECT_DOUBLE_EQ(heir.calculate_raw_net_demand(3.0), 0.0);

    heir.state = HeirState::MINOR;
    EXPECT_DOUBLE_EQ(heir.calculate_raw_net_demand(3.0), 0.0);

    heir.state = HeirState::DECEASED;
    EXPECT_DOUBLE_EQ(heir.calculate_raw_net_demand(3.0), 0.0);
}


TEST(BeneficiaryTest, ActiveStateCalculatesNetDemand) {
    Beneficiary heir(1001, 25, 1);
    heir.deposit_capital(10'000.0); // State = ACTIVE

    // Multiplier = 3.0 -> Net Match = (3.0 - 1.0) * $10,000 = $20,000
    EXPECT_DOUBLE_EQ(heir.calculate_raw_net_demand(3.0), 20'000.0);

    // Multiplier = 1.5 -> Net Match = (1.5 - 1.0) * $10,000 = $5,000
    EXPECT_DOUBLE_EQ(heir.calculate_raw_net_demand(1.5), 5'000.0);
}

TEST(BeneficiaryTest, MultiplierLessThanOrEqualToOneYieldsZeroDemand) {
    Beneficiary heir(1001, 25, 1);
    heir.deposit_capital(10'000.0); // State = ACTIVE

    // Multiplier <= 1.0 provides no bonus match
    EXPECT_DOUBLE_EQ(heir.calculate_raw_net_demand(1.0), 0.0);
    EXPECT_DOUBLE_EQ(heir.calculate_raw_net_demand(0.5), 0.0);
}

