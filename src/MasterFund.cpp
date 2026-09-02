#include "MasterFund.hpp"
#include "DemographicEngine.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

MasterFund::MasterFund(double start_aum, 
                       const MarketEngine& market, 
                       const PolicyEngine& policy)
    : initial_aum(start_aum),
      current_aum(start_aum),
      current_year(0),
      market_sim(market),
      policy_engine(policy) {}

void MasterFund::step_annual_tick() {
    ++current_year;

    // 1. Snapshot start of year state
    const double starting_aum = current_aum;

    // 2. Generate market growth multiplier and update AUM directly
    const double multiplier = market_sim.generate_annual_multiplier();
    current_aum = starting_aum * multiplier;
    const double annual_return = multiplier - 1.0;

    // 3. Compute global cap available for distribution
    const double global_cap = policy_engine.calculate_global_cap(current_aum);

    // 4. Ensure per-stirpes branch shares are dynamically rebalanced
    lineages.rebalance_per_stirpes_shares();

    // 5. Execute 3-Phase Settlement across all family lineages
    const double total_disbursed = lineages.execute_annual_settlement(global_cap, policy_engine);

    // 6. Deduct actual cash payouts from Master Fund AUM
    current_aum -= total_disbursed;

    // 7. Assemble Annual Snapshot
    AnnualSnapshot snap;
    snap.year = current_year;
    snap.starting_aum = starting_aum;
    snap.ending_aum = current_aum;
    snap.annual_market_return = annual_return;
    snap.global_cap_dollars = global_cap;

    // Collect aggregate phase disbursements
    snap.total_base_disbursed = 0.0;
    snap.total_spillover_disbursed = 0.0;
    for (const auto& heir : lineages.get_beneficiaries()) {
        snap.total_base_disbursed += heir.last_approved_base_payout;
        snap.total_spillover_disbursed += heir.last_approved_spillover_payout;
    }
    snap.unused_surplus_retained = std::max(0.0, global_cap - total_disbursed);

    // 8. Capture detailed branch and heir snapshots
    lineages.capture_telemetry_snapshot(snap, global_cap, policy_engine);

    // 9. Move snapshot into Telemetry Logger buffer (0 copies)
    telemetry.record_year(std::move(snap));

}

void MasterFund::run_simulation_json(uint32_t total_years, DemographicEngine &demo_engine, const std::string &output_json_path)
{
    if (total_years == 0) return;
    
    telemetry.reserve_years(total_years);
    // Dynamically balance the tree shares between beneficiary
    lineages.rebalance_per_stirpes_shares();

    for (uint32_t y = 0; y < total_years; ++y) {
        demo_engine.step_demographics(*this);
        step_annual_tick();
    }
    // export to json
    if (!output_json_path.empty()) {
        if(telemetry.export_to_json(output_json_path)) {
            std::cout << "[MasterFund] Simulation complete (" << total_years 
                      << " years). Telemetry exported to: " << output_json_path << "\n";
        }
        else {
            std::cerr << "[MasterFund] Warning: Failed to export telemetry to: " 
                      << output_json_path << "\n";
        }
    }
}

void MasterFund::run_simulation_binary(uint32_t total_years, DemographicEngine &demo_engine, const std::string &output_bin_path)
{
    if (total_years == 0) return;
    
    telemetry.reserve_years(total_years);
    // Dynamically balance the tree shares between beneficiary
    lineages.rebalance_per_stirpes_shares();

    for (uint32_t y = 0; y < total_years; ++y) {
        demo_engine.step_demographics(*this);
        step_annual_tick();
    }
    if (!output_bin_path.empty()) {
        if (telemetry.export_to_binary_mmap(output_bin_path)) {
            std::cout << "[MasterFund] Simulation complete (" << total_years
                      << " years). Binary telemetry exported to: " << output_bin_path << "\n";
        } else {
            std::cerr << "[MasterFund] Warning: Failed to export binary telemetry to: "
                      << output_bin_path << "\n";
        }
    }
}

void MasterFund::run_simulation_both(uint32_t total_years, DemographicEngine &demo_engine,
                                     const std::string &output_json_path,
                                     const std::string &output_bin_path)
{
    if (total_years == 0) return;
    
    telemetry.reserve_years(total_years);
    // Dynamically balance the tree shares between beneficiary
    lineages.rebalance_per_stirpes_shares();

    for (uint32_t y = 0; y < total_years; ++y) {
        demo_engine.step_demographics(*this);
        step_annual_tick();
    }
    // export to json
    if (!output_json_path.empty()) {
        if(telemetry.export_to_json(output_json_path)) {
            std::cout << "[MasterFund] Simulation complete (" << total_years 
                      << " years). Telemetry exported to: " << output_json_path << "\n";
        }
        else {
            std::cerr << "[MasterFund] Warning: Failed to export telemetry to: " 
                      << output_json_path << "\n";
        }
    }
    if (!output_bin_path.empty()) {
        if (telemetry.export_to_binary_mmap(output_bin_path)) {
            std::cout << "[MasterFund] Simulation complete (" << total_years
                      << " years). Binary telemetry exported to: " << output_bin_path << "\n";
        } else {
            std::cerr << "[MasterFund] Warning: Failed to export binary telemetry to: "
                      << output_bin_path << "\n";
        }
    }
}
void MasterFund::reset() noexcept {
        current_aum = initial_aum;
        current_year = 0;
        telemetry.clear();
}