#include "MasterFund.hpp"
#include "DemographicEngine.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <utility>

template <typename Func, typename... Args>
double measure_execution_ms(Func&& func, Args&&... args) {
    const auto start = std::chrono::steady_clock::now();
    
    std::forward<Func>(func)(std::forward<Args>(args)...);
    
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}

int main () {
    static_assert(sizeof(Beneficiary) == 64);
    static_assert(sizeof(MarketEngine) == 2560);
    static_assert(sizeof(PolicyConfig) == 32);
    static_assert(sizeof(BranchNode) == 32);
    static_assert(sizeof(LineageRegistry) == 56);
    static_assert(sizeof(HeirSnapshot) == 56);
    static_assert(sizeof(BranchSnapshot) == 48);
    static_assert(sizeof(AnnualSnapshot) == 112);
    static_assert(sizeof(MasterFund) == 2696);

    std::cout << "=========================================================\n";
    std::cout << "      MASTER FUND MONTE CARLO SIMULATION ENGINE          \n";
    std::cout << "=========================================================\n\n";

    // 1. Initialize Market Engine (10% target CAGR, 15% annual volatility)
    MarketEngine market(0.10, 0.15, 42);

    // 2. Initialize Policy Engine (3.0% Global Cap, 3.0x Match Multiplier, 1.5x Spillover Cap)
    PolicyConfig config {
        .net_match_multiplier = 3.0,
        .global_cap_ratio = 0.03,
        .spillover_cap_ratio = 1.50,
        .age_of_majority = 21,
        .mode = GovernanceMode::HYBRID_SPILLOVER
    };
    PolicyEngine policy(config);

    // 3. Construct Master Fund with $10,000,000 Starting AUM
    double initial_aum = 50'000'000.00;
    MasterFund fund(initial_aum, market, policy);

    // 4. Configure & Initialize Demographic Engine
    DemographicConfig demo_config {
        .initial_branches = 100,
        .initial_heirs = 250,
        .annual_birth_rate = 0.03,        // ~3% annual birth chance
        .annual_branch_rate = 0.05,       // ~5% chance per year to branch off
        .contribution_volatility = 0.20,  // 20% annual contribution shock variance
        .seed = 1337
    };
    DemographicEngine demo_engine(demo_config);
    // 5. Seed Initial Family Topology & Population via Public MasterFund API
    demo_engine.seed_estate(fund);

    const uint32_t SIMULATION_YEARS = 100;

    // 6. Run 30-Year Simulation
    std::cout << "[Setup] Initial AUM:             $" << std::fixed << std::setprecision(2) << initial_aum << "\n";
    std::cout << "[Setup] Active Lineage Branches: " << fund.get_lineages().branch_count() << "\n";
    std::cout << "[Setup] Active Beneficiaries:    " << fund.get_lineages().beneficiary_count() << "\n";
    std::cout << "\n[Engine] Running " << SIMULATION_YEARS << "-year dynamic simupplation...\n\n";

    // double elapsed_ms = measure_execution_ms([&]() {
    //     fund.run_simulation_json(
    //         SIMULATION_YEARS,
    //         demo_engine, 
    //         "./simulation_telemetry.json"
    //     );
    // });

    double elapsed_ms = measure_execution_ms([&]() {
        fund.run_simulation_binary(
            SIMULATION_YEARS,
            demo_engine, 
            "./simulation_telemetry.bin"
        );
    });
    // double elapsed_ms = measure_execution_ms([&]() {
    //     fund.run_simulation_both(
    //         SIMULATION_YEARS,
    //         demo_engine, 
    //         "./simulation_telemetry.json",
    //         "./simulation_telemetry.bin"
    //     );
    // });

    std::cout << "\n=========================================================\n";
    std::cout << "                  SIMULATION SUMMARY                     \n";
    std::cout << "=========================================================\n";
    std::cout << "Ending AUM:               $" << std::fixed << std::setprecision(2) << fund.get_current_aum() << "\n";
    std::cout << "Total Years Simulated:    " << fund.get_current_year() << "\n";
    std::cout << "Final Active Branches:    " << fund.get_lineages().branch_count() << "\n";
    std::cout << "Final Beneficiaries:      " << fund.get_lineages().beneficiary_count() << "\n";
    std::cout << "=========================================================\n";

    std::cout << "=========================================================\n";
    std::cout << "                  BENCHMARK RESULTS                      \n";
    std::cout << "=========================================================\n";
    std::cout << "Simulated Years:     " << SIMULATION_YEARS << "\n";
    std::cout << "Elapsed Time:        " << std::fixed << std::setprecision(4) << elapsed_ms << " ms\n";
    std::cout << "Throughput:          " << (SIMULATION_YEARS / (elapsed_ms / 1000.0)) << " years/sec\n";
    std::cout << "=========================================================\n";

    return 0;


}