#pragma once

#include "MarketEngine.hpp"
#include "PolicyEngine.hpp"
#include "LineageRegistry.hpp"
#include "TelemetryLogger.hpp"

#include <string>
#include <cstdint>
#include <functional>

class DemographicEngine;

class MasterFund {
private:
    double   initial_aum  {0.0f};
    double   current_aum  {0.0f};
    uint32_t current_year {0};

    MarketEngine    market_sim;
    PolicyEngine    policy_engine;
    LineageRegistry lineages;
    TelemetryLogger telemetry;

public:
    MasterFund() = default;
    MasterFund(double start_aum, const MarketEngine& market, const PolicyEngine& policy);

    void step_annual_tick();

    // Runs a full multi-year simulation and optionally flushes JSON telemetry to disk
    void run_simulation_json(uint32_t total_years, DemographicEngine& demo_engine, const std::string& output_json_path);
    void run_simulation_binary(uint32_t total_years, DemographicEngine& demo_engine, const std::string& output_bin_path);
    void run_simulation_both(uint32_t total_years, DemographicEngine& demo_engine,
                        const std::string& output_json_path,
                        const std::string& output_bin_path);

    void reset() noexcept;

    uint32_t create_root_branch(uint32_t branch_id) {
        return lineages.create_root_branch(branch_id);
    }
    
    uint32_t add_sub_branch(uint32_t parent_index, uint32_t branch_id) {
        return lineages.add_sub_branch(parent_index, branch_id);
    }

    uint32_t add_beneficiary(uint32_t branch_index, 
                            uint64_t beneficiary_id, 
                            uint16_t age, 
                            double annual_contribution) {
        return lineages.add_beneficiary(branch_index, beneficiary_id, age, annual_contribution);
    }

    // --- ACCESSORS & MUTATORS ---

    void set_initial_aum(double aum) noexcept { initial_aum = aum; current_aum = aum; }
    void set_market_engine(const MarketEngine& market) noexcept { market_sim = market; }
    void set_policy_engine(const PolicyEngine& policy) noexcept { policy_engine = policy; }

    [[nodiscard]] double get_current_aum() const noexcept { return current_aum; }
    [[nodiscard]] double get_initial_aum() const noexcept { return initial_aum; }
    [[nodiscard]] uint32_t get_current_year() const noexcept { return current_year; }

    [[nodiscard]] const LineageRegistry& get_lineages() const noexcept { return lineages; }
    [[nodiscard]] LineageRegistry& get_lineages_mut() noexcept { return lineages; }

    [[nodiscard]] const PolicyEngine& get_policy_engine() const noexcept { return policy_engine; }
    [[nodiscard]] const TelemetryLogger& get_telemetry() const noexcept { return telemetry; }
};
