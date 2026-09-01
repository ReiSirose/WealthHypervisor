# **MasterFund: High-Performance Wealth Inheritance Hypervisor**

A cache-friendly, Data-Oriented Design (DOD) simulation engine engineered for high throughput, flat contiguous memory layouts, and zero virtual-table pointer (vptr) overhead.

## Concept: The Wealth Hypervisor

The **Wealth Hypervisor** acts as an automated, impartial family referee for wealth governance. It enforces clear, algorithmic rules to ensure multi-generational legacy preservation, eliminating bad spending habits and family disputes.

### Core Governance

* **Contributory Earn-Out & Multiplier Payout**
  * **Earn-to-Withdraw**: The *only* mechanism for a beneficiary to unlock fund distributions is by actively contributing capital into the fund. To earn money, you must make money.
  * **Policy Multiplier**: Distributions match contributions up to the configured policy net multiplier. For example, a $3.0\times$ multiplier on a $\$10,000$ personal contribution yields a $\$30,000$ total payout (returning the $\$10,000$ principal alongside $\$20,000$ of fund growth).
  * **Tax Efficiency & Incentive Design**: Returning principal alongside capital distributions optimizes tax efficiency while reinforcing the psychological principle that wealth extraction requires personal productivity.

* **Protecting the Golden Goose & Zero Favoritism**
  * **Vault Protection via Distribution Caps**: All distributions are strictly capped at 3% of current AUM annually. Combined with per-stirpes branch ceilings and individual multiplier limits, this ensures the fund's principal is never over-depleted in any single year.
  * **Deterministic Execution**: Eliminates subjective approvals, manual requests, and family drama—distributions execute strictly based on code policy, state math, and verified metrics.
  * **No Automatic Emergency Coverage**: No loop-hole for excused emergency coverage because it eliminate the prudence from risk assessment.

* **Perpetual Capital Solvency (Infinite Runway)**
  * **Stochastic Stress-Testing**: `MarketEngine` models multi-decade asset growth simulating standard S&P 500 market dynamics (~10% average annual returns) via log-normal drift $e^{r_t}$ with Itô correction.
  * **Long-Term Solvency Protection**: By capping annual distributions at 3% of AUM and assuming positive market returns, the engine provides strong protection against capital depletion across multi-generational horizons. With 10% average S&P 500 returns and 15% volatility, mathematical modeling demonstrates high probability (99%+ confidence) of perpetual solvency.

---

### Prerequisites
* **Compiler**: C++20 compliant compiler (GCC 10+, Clang 12+, or MSVC 2019+)
* **Build System**: CMake 3.20 or higher

### Build Steps

```bash
# 1. Configure the project build tree in Release mode
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 2. Compile the executable using all available cores
cmake --build build --config Release -j

# 3. Run the simulation binary
./build/fund_sim
```

## **Architecture Overview**

The engine processes annual simulation ticks through a **3-Phase Settlement Cycle** orchestrated by `MasterFund`, with demographic mutations handled in parallel by `DemographicEngine`.

```text
========================================================================================
                                MASTERFUND SYSTEM ARCHITECTURE
========================================================================================

                      +-----------------------------+
                      |      MasterFund             |
                      |  (Annual Simulation Tick)   |
                      +----------+------------------+
                                 |         ╭─────────────────────────────╮
                                 |         │  1. MarketEngine: Growth     │
                                 |         │  2. PolicyEngine: Caps       │
                                 |         │  3. LineageRegistry: Settle  │
                                 |         │  4. TelemetryLogger: Record  │
                                 |         ╰─────────────────────────────╯
      ┌──────────────┬────────────┼────────────┬──────────────┬──────────┐
      v              v            v            v              v          v
+----------+   +-----------+  +-----------+  +-----------+  +------+  +---------+
|  Market  |   | PolicyCfg |  | Lineage   |  | Telemetry |  | Demo |  |AnnualSnap|
|  Engine  |   |  Engine   |  | Registry  |  |  Logger   |  | Eng. |  | (DTO)    |
+----------+   +-----------+  +-----------+  +-----------+  +------+  +---------+
| PRNG     |   | Modes:    |  | Phase 1:  |  | History   |  | Births|  Year/AUM |
| (MT19937)|   |  STRICT   |  |  Base Cap |  |  Snapshots|  | Deaths| Payouts  |
| Seed     |   |  DYNAMIC  |  | Phase 2:  |  | (move)    |  | Aging | Branches |
| CAGR     |   |  HYBRID   |  |  Inherit  |  | JSON Exp. |  | Branch| Heirs    |
| Volatil. |   | Multipier |  | Phase 3:  |  |           |  | Cretn |          |
+----------+   | Caps      |  |  Spillover|  +-----------+  +------+  +---------+
               +-----------+  +-----------+
                                  |
                  ┌───────────────┴────────────────┐
                  │  Flat Contiguous Arenas       │
                  ├───────────────────────────────┤
                  │ BranchNode:  32-byte aligned  │
                  │  - LCRS tree indices (no ptrs)│
                  │  - Per-stirpes share ratios   │
                  │                               │
                  │ Beneficiary: 64-byte aligned  │
                  │  - HeirState (MINOR/ACTIVE)   │
                  │  - Capital contributions      │
                  │  - Payout history             │
                  └───────────────────────────────┘
```


### **Core Components**

| Component | Responsibility | Implementation Details |
| :---- | :---- | :---- |
| **MasterFund** | Global simulation orchestrator. | Owns all sub-engines. Each `step_annual_tick()` executes: (1) market growth, (2) per-stirpes rebalancing, (3) 3-phase settlement, (4) demographic mutations, (5) telemetry capture. Tracks: initial\_aum, current\_aum, current\_year. |
| **MarketEngine** | S\&P 500 stochastic drift. | MT19937 PRNG applying log-normal drift $e^{r_t}$ with Itô correction. Configurable seed, CAGR target (~10%), annual volatility (~15%). Size: ~2.5 KB. |
| **PolicyEngine** | Governance rules & cap enforcement. | Stateless rule engine with **3 governance modes** (STRICT\_PARTITION, DYNAMIC\_POOLING, HYBRID\_SPILLOVER). Enforces: 3.0x match multiplier, 3.0% global cap ratio, 1.5x base spillover ceiling, age-of-majority rules. |
| **LineageRegistry** | Tree hierarchy & settlement engine. | Owns two flat contiguous arenas: `branch_arena` (LCRS trees) and `beneficiary_arena` (heir PODs). Implements **3-phase per-stirpes settlement**, per-stirpes rebalancing, and branch traversal without dynamic allocation. |
| **DemographicEngine** | Stochastic lifecycle & family growth. | Seeds initial family topology and mutates via: birth events, aging/death transitions, branch creation. O(1) registration into registry arenas. Configurable rates: birth, branching, contribution volatility. |
| **TelemetryLogger** | State snapshots & telemetry export. | Double-buffered capture of annual snapshots (year, AUM, phase payouts, heir/branch details). Exports to JSON (optional) or binary format. Zero-copy move semantics. |
| **AnnualSnapshot** | Transient DTO capturing year state. | POD struct recording: year, starting\_aum, ending\_aum, global\_cap, phase disbursements, branch/heir snapshots. Assembled during settlement, moved into TelemetryLogger history. |
| **HeirSnapshot** | Per-beneficiary annual record. | Captures: heir\_id, branch\_id, age, capital\_contribution, raw\_match\_demand, base\_payout, spillover\_payout, unmet\_demand. Used for telemetry and debugging. |
| **BranchSnapshot** | Per-branch annual aggregate. | Captures: branch\_id, virtual\_share\_percentage, base\_cap, base\_disbursed, spillover\_disbursed, active\_heir\_count. Reflects branch-level settlement totals. |

## **Memory Layout & Cache Alignment**

By favoring flat arrays over class hierarchies, objects avoid 8-byte vptr headers and align cleanly to CPU cache line boundaries. All POD structs are tightly packed with explicit alignment directives.

### **LCRS Tree Explanation**

**Left-Child Sibling-Right (LCRS)** is a pointer-free tree representation using indices into a flat arena:
- Each node stores `first_child_index` (pointing to leftmost child) and `next_sibling_index` (pointing to right sibling)
- Eliminates pointer dereferencing; all tree traversal uses array indices (cache-friendly)
- Example family tree: `parent → child_0 → child_1 → child_2` becomes contiguous indices without heap allocations

### **BranchNode Data POD (32 Bytes / Cache Aligned)**

```
struct alignas(32) BranchNode {
    uint32_t branch_id;              // 4B: Unique branch identifier
    uint32_t parent_index;           // 4B: Parent arena index (INVALID_INDEX = Root)
    double virtual_share_percentage; // 8B: Per-stirpes share ratio (e.g., 0.50 = 50%)
    uint32_t first_child_index;      // 4B: LCRS: index to leftmost child
    uint32_t next_sibling_index;     // 4B: LCRS: index to next right sibling
    uint16_t child_count;            // 2B: Total direct children
    uint16_t active_heir_count;      // 2B: Active contributing heirs in this branch
    uint32_t heir_start_index;       // 4B: Start index in global beneficiary_arena
};  // Total: 32 bytes, cache-line aligned
```

### **Beneficiary Data POD (64 Bytes / L1 Cache Line Aligned)**

```
struct alignas(64) Beneficiary {
    uint64_t id;                              // 8B: Unique heir ID
    double annual_capital_contribution;       // 8B: Capital committed annually
    double last_approved_base_payout;         // 8B: Phase 1 approved payout
    double last_approved_spillover_payout;    // 8B: Phase 3 spillover allocation
    uint32_t branch_id;                       // 4B: Branch association
    uint32_t branch_index;                    // 4B: Index into branch_arena
    uint16_t age;                             // 2B: Current age
    HeirState state;                          // 1B: MINOR | INACTIVE | ACTIVE | DECEASED
    uint8_t padding;                          // 1B: Reserved
};  // Total: 64 bytes, L1 cache-line aligned, auto-SIMD vectorizable
```

### **HeirState Enumeration**

The `HeirState` enum defines the lifecycle state of each beneficiary:

```
enum class HeirState : uint8_t {
    MINOR    = 0,   // Age < 21: Cannot submit contributions or receive payouts
    INACTIVE = 1,   // Age >= 21, no contributions: "skin in the game" requirement unmet
    ACTIVE   = 2,   // Age >= 21, contributions > 0: Eligible for 3x match
    DECEASED = 3    // Terminal state: Triggers automatic rebalancing
};
```

**State Transitions:**
- `MINOR` → `INACTIVE` on 21st birthday (automatic age tick)
- `INACTIVE` → `ACTIVE` when first capital contribution received
- `ACTIVE` → `INACTIVE` if contributions drop to zero
- Any state → `DECEASED` (one-way transition, triggers per-stirpes rebalance)

### **Struct Size Reference**

Key struct sizes (verified by `static_assert` in main.cpp):

| Struct | Size | Purpose |
| :---- | :---- | :---- |
| `Beneficiary` | 64 bytes | Cache-line aligned heap POD |
| `BranchNode` | 32 bytes | LCRS tree node |
| `HeirSnapshot` | 56 bytes | Annual heir telemetry |
| `BranchSnapshot` | 48 bytes | Annual branch telemetry |
| `AnnualSnapshot` | 112 bytes | Complete year snapshot |
| `PolicyConfig` | 32 bytes | Governance rules |
| `MarketEngine` | 2560 bytes | MT19937 PRNG engine |
| `LineageRegistry` | 56 bytes | Two vector pointers + scalar |
| `MasterFund` | 2696 bytes | Aggregates all engines |

## **Governance Modes & Policy Enforcement**

### **PolicyEngine Governance Modes**

The `PolicyEngine` supports three distinct governance strategies, each with different wealth distribution philosophy:

#### **1. STRICT_PARTITION (Hard Ceiling)**
Each branch receives a fixed entitlement cap based on per-stirpes share:
```
Branch Ceiling = Global Cap × Branch Share (no spillover allowed)
```
- Pros: Predictable, prevents one branch from claiming excess
- Cons: Unused capital in underfunded branches cannot flow to others

#### **2. DYNAMIC_POOLING (Full Commons)**
All branches compete for the global cap without hard ceilings:
```
Branch Ceiling = Global Cap (can claim up to 100% if demand exists)
```
- Pros: Maximizes capital deployment based on actual contribution demand
- Cons: High-performing branches can monopolize payouts

#### **3. HYBRID_SPILLOVER (Recommended - Default)**
Each branch gets base entitlement + bounded spillover allowance:
```
Branch Ceiling = (Global Cap × Branch Share) × Spillover Cap Ratio (default: 1.5x)
Example: $300k global cap × 0.50 share × 1.5 spillover = $225k effective ceiling
```
- Pros: Balances fairness with capital efficiency; unused base entitlements "spill" to active branches
- Cons: Slightly more complex settlement logic

### **Per-Stirpes Rebalancing**

**Per-stirpes** (by family branch) is a legal inheritance principle ensuring equitable wealth distribution across family lines. The engine implements this via dynamic share rebalancing:

1. **Initial Share Assignment**: Each branch receives a virtual\_share\_percentage based on its depth in the family tree
2. **Rebalancing Trigger**: Occurs annually before settlement when demographic changes happen (births, deaths, new branches)
3. **Recalculation**: Shares are recomputed top-down to reflect current branch structure
4. **Result**: Each beneficiary's effective allocation accounts for sibling, cousin, and generational fairness

This prevents scenarios where a branch with no active heirs "starves" while a sibling branch over-distributes.

### **3-Phase Settlement Algorithm**

Each annual tick executes a deterministic 3-phase settlement cycle. All phases operate within the global cap computed as `Global Cap = Current AUM × 3.0%`.

#### **Phase 1: Base Entitlement (Per-Stirpes Guaranteed)**
```
For each beneficiary in ACTIVE state:
  raw_demand = annual_capital_contribution × 3.0 (net multiplier)
  branch_share = branch.virtual_share_percentage
  base_allocation = global_cap × branch_share
  heir_cap = base_allocation / branch.active_heir_count
  base_payout = MIN(raw_demand, heir_cap)
  cash_disbursed += base_payout
```
Guarantees each active heir receives either their full 3x match or their per-stirpes fair share, whichever is smaller.

#### **Phase 2: Inheritance Queue (Future Expansion)**
Reserved for deceased heir wealth redistribution. Currently a placeholder.

#### **Phase 3: Spillover Distribution (Opportunistic)**
```
Remaining_surplus = global_cap - cash_disbursed_phase1
For each branch with demand > base_allocation:
  spillover_ceiling = (base_allocation × spillover_cap_ratio) - base_payout
  additional_demand = MAX(0, raw_demand - base_payout)
  spillover_payout = MIN(additional_demand, spillover_ceiling, remaining_surplus)
  cash_disbursed += spillover_payout
  remaining_surplus -= spillover_payout
```
Distributes unused capital from over-entitled branches to under-funded active heirs, up to 1.5x their base ceiling.

**Result**: By end of phase 3, total payouts remain ≤ global\_cap, ensuring perpetual capital solvency.

## **Performance Profiling & Hardware Counters**

Baseline profiling reveals the runtime performance, bottlenecks, and execution characteristics of the application based on CPU and time profiler metrics.

### **Time & Cycle Distribution**

| Execution Target | Time (ms) | Time % | CPU Cycles | Cycle % |
| :---- | :---- | :---- | :---- | :---- |
| **TelemetryLogger::export\_to\_json** | **10.10 ms** | **76.5%** | **166.20 M** | **83.3%** |
| **MasterFund::step\_annual\_tick** (all 3 phases + rebalance) | 1.70 ms | 12.9% | 21.05 M | 10.6% |
| **DemographicEngine::step\_demographics** | 0.40 ms | 3.0% | 6.85 M | 3.4% |
| **Total Engine Run** | **13.20 ms** | **100.0%** | **199.50 M** | **100.0%** |

### **Hardware Counter Summary**

> * **Cache Efficiency**:  
  * L1D Load Misses: 92,985  
  * L1D Store Misses: 62,069  
  * L1D TLB Misses: 30,359  
> * **Branching Performance**:  
  * Total Branches: 171,863,105 (131,708,369 Taken)  
  * **Incorrectly Predicted Conditional Branches**: 438,146  
  * Unpredicted Memory Dependencies: 32,736  
> * **Vectorization Metrics**:  
  * SIMD Vector Arithmetic Operations: 6,405,580

## **MarketEngine: Stochastic Asset Growth Simulation**

The `MarketEngine` uses a Mersenne Twister PRNG to simulate realistic market returns via log-normal distribution:

### **Configuration Parameters**

```cpp
struct MarketEngine {
    double target_cagr;           // Compound Annual Growth Rate (~10% for S&P 500)
    double std_dev;               // Annual volatility (~15%)
    uint32_t seed;                // PRNG seed (critical for reproducibility)
    std::mt19937 rng;             // MT19937 random number generator (~2.5 KB)
    std::normal_distribution<> dist;  // Normal dist. for log-returns
};
```

### **Annual Growth Calculation**

Each year generates a market multiplier via log-normal drift with Itô correction:

$$\text{log\_mean} = \ln(1 + \text{CAGR}) - \frac{1}{2}\sigma^2$$
$$\text{annual\_multiplier} = e^{\mathcal{N}(\text{log\_mean}, \sigma)}$$
$$\text{new\_AUM} = \text{current\_AUM} \times \text{annual\_multiplier}$$

This ensures:
- **Long-term drift** toward target CAGR (unbiased log-normal expectation)
- **Positive portfolio values** (exponential ensures returns never drop below $0)
- **Realistic volatility clustering** (consecutive years correlated via PRNG seed)

### **Example: 100-Year Simulation Setup**

```cpp
// Initialize market with 10% CAGR, 15% volatility, fixed seed
MarketEngine market(0.10, 0.15, 42);

// Seeds are critical: same seed produces identical simulation
market.set_seed(42);  // Reproducible Monte Carlo
```

## **DemographicEngine: Family Growth & Lifecycle**

The `DemographicEngine` drives stochastic family evolution, mutating beneficiary states and branch topology annually.

### **Configuration Parameters**

```cpp
struct DemographicConfig {
    size_t initial_branches;        // Initial number of family branches (e.g., 100)
    size_t initial_heirs;           // Initial beneficiary population (e.g., 250)
    double annual_birth_rate;       // Probability new heir born each year (~3%)
    double annual_branch_rate;      // Probability new branch created (~5%)
    double contribution_volatility; // Annual contribution shock variance (~20%)
    uint32_t seed;                  // PRNG seed for reproducibility
};
```

### **Lifecycle Mutations**

#### **Annual Aging**
- Each beneficiary's age increments by 1
- On 21st birthday: `MINOR` → `INACTIVE` (automatic)
- On death (stochastic): Any state → `DECEASED` (triggers per-stirpes rebalance)

#### **Birth Events**
- With probability `annual_birth_rate` per year, new heirs are randomly added to existing branches
- New heirs enter as `MINOR` state (age 0)
- Automatically transition to `INACTIVE` on 21st birthday

#### **Branch Creation**
- With probability `annual_branch_rate`, new sub-branches split from existing branches
- New branches inherit virtual share ratios via rebalancing
- Enables multi-generational family tree growth

#### **Contribution Volatility**
- Each active heir's annual contribution subject to shock: `new_contribution = old_contribution × (1 + N(0, σ))`
- Simulates real-world income variability (job loss, salary raise, etc.)

### **Seeding Initial Estate**

```cpp
DemographicConfig demo_config {
    .initial_branches = 100,
    .initial_heirs = 250,
    .annual_birth_rate = 0.03,
    .annual_branch_rate = 0.05,
    .contribution_volatility = 0.20,
    .seed = 1337
};

DemographicEngine demo_engine(demo_config);
demo_engine.seed_estate(fund);  // Populates fund with initial branches and heirs
```

## **Telemetry & Snapshot Capture**

### **Telemetry Capture During Settlement**

After each annual tick, `MasterFund::step_annual_tick()` assembles a comprehensive `AnnualSnapshot` capturing:

**Year-Level Aggregates:**
- `year`: Simulation year number
- `starting_aum` / `ending_aum`: Fund value before and after market growth and payouts
- `annual_market_return`: Growth multiplier (as percentage)
- `global_cap_dollars`: Annual distribution budget (3% of AUM)
- `total_base_disbursed`: Sum of all Phase 1 payouts
- `total_spillover_disbursed`: Sum of all Phase 3 payouts
- `unused_surplus_retained`: Unclaimed capital (reinvested in principal)

**Branch-Level Snapshots:**
- `vector<BranchSnapshot>`: One entry per active branch containing:
  - `branch_id`, `virtual_share_percentage`, `base_cap_dollars`
  - `base_disbursed`, `spillover_disbursed`, `active_heir_count`

**Heir-Level Snapshots:**
- `vector<HeirSnapshot>`: One entry per beneficiary containing:
  - `heir_id`, `age`, `branch_id`, `state`
  - `capital_contribution`, `raw_match_demand`, `base_payout`, `spillover_payout`
  - `unmet_demand`: Clipped requests (demand exceeding caps)

### **Export & Logging**

The `TelemetryLogger` stores annual snapshots in a `vector<AnnualSnapshot>` and offers two export modes:

1. **JSON Export** (optional): `telemetry.export_to_json("output.json")`
   - Human-readable, self-documenting format
   - Currently the performance bottleneck (83% of cycles)
   - Called at simulation end (non-blocking)

2. **Memory Resident**: Snapshots remain in `telemetry.get_history()` for in-process analysis
   - Zero-copy access via `std::move` semantics
   - Fast for statistical aggregation (mean AUM growth, payout variance, etc.)

## **Complete Usage Example**

### **Run a 100-Year Simulation**

```cpp
#include "MasterFund.hpp"
#include "DemographicEngine.hpp"

int main() {
    // 1. Initialize Market Engine (10% CAGR, 15% volatility, seed=42)
    MarketEngine market(0.10, 0.15, 42);

    // 2. Initialize Policy Engine (3.0% cap, 3.0x multiplier, 1.5x spillover, HYBRID mode)
    PolicyEngine policy(PolicyConfig{
        .net_match_multiplier = 3.0,
        .global_cap_ratio = 0.03,
        .spillover_cap_ratio = 1.50,
        .age_of_majority = 21,
        .mode = GovernanceMode::HYBRID_SPILLOVER
    });

    // 3. Create Master Fund with $50,000,000 initial AUM
    MasterFund fund(50'000'000.0, market, policy);

    // 4. Initialize Demographic Engine
    DemographicEngine demo_engine(DemographicConfig{
        .initial_branches = 100,
        .initial_heirs = 250,
        .annual_birth_rate = 0.03,
        .annual_branch_rate = 0.05,
        .contribution_volatility = 0.20,
        .seed = 1337
    });

    // 5. Seed initial family topology
    demo_engine.seed_estate(fund);

    // 6. Run 100-year simulation with optional JSON export
    fund.run_simulation(
        100,                           // Total years
        demo_engine,                   // Demographic mutations
        "./simulation_telemetry.json"  // Optional export (can be empty string to skip)
    );

    // 7. Access results
    std::cout << "Ending AUM: $" << fund.get_current_aum() << "\n";
    std::cout << "Final Branches: " << fund.get_lineages().branch_count() << "\n";
    std::cout << "Final Beneficiaries: " << fund.get_lineages().beneficiary_count() << "\n";

    // 8. Analyze in-memory telemetry (without JSON file I/O)
    const auto& history = fund.get_telemetry().get_history();
    for (size_t i = 0; i < history.size(); i++) {
        const auto& snap = history[i];
        std::cout << "Year " << snap.year << ": AUM=$" 
                  << snap.ending_aum << ", Disbursed=$" 
                  << (snap.total_base_disbursed + snap.total_spillover_disbursed) << "\n";
    }

    return 0;
}
```

**Console Output:**
```
=========================================================
      MASTER FUND MONTE CARLO SIMULATION ENGINE          
=========================================================

[Setup] Initial AUM:             $50,000,000.00
[Setup] Active Lineage Branches: 100
[Setup] Active Beneficiaries:    250

[Engine] Running 100-year dynamic simulation...

=========================================================
                  SIMULATION SUMMARY                     
=========================================================
Ending AUM:               $XXX,XXX,XXX.XX
Total Years Simulated:    100
Final Active Branches:    150+  (grown via demographic mutations)
Final Beneficiaries:      500+  (births + new entrants)
=========================================================

Elapsed Time:        X.XXXX ms
Throughput:          XXX.XX years/sec
```

### **Build & Run**

```bash
# Configure Release build with optimizations
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Compile all targets (fund_sim, fund_sim_profile)
cmake --build build --config Release -j

# Run main simulation (includes optional JSON export)
./build/fund_sim

# Run profiling build (with CPU cycle counters)
./build/fund_sim_profile

# Analyze telemetry JSON (post-simulation)
python3 analyze_telemetry.py simulation_telemetry.json
```

## **Planned Optimizations**

### **1. JSON Export Overhaul (Target: ~80% Cycle Reduction)**

> * **Problem**: TelemetryLogger::export\_to\_json consumes **83.3% of CPU cycles** (166.2M cycles) and 76.5% of total runtime due to standard string stream formatting and synchronous I/O operations.  
> * **Solution**:  
  * Replace std::ofstream << operator with memory-mapped file I/O, bypassing expensive write() syscalls
  * Use raw `std::memcpy` for binary POD serialization instead of slow string formatting and heap allocation
  * Completely eliminate JSON generation from the engine hot path during simulation
> * **Proposed Implementation**:
  * **Memory-Mapped Storage**: Preallocate binary file using `mio::mmap_sink` (or platform equivalents)
  * **Direct POD Copy**: Serialize packed `AnnualSnapshot` structs via `std::memcpy` into mapped memory, achieving O(1) write time
  * **Post-Simulation Export** (optional): Generate human-readable JSON separately after simulation completes (non-critical path)
  * **Python Ingestion**: Load binary telemetry in Python via `numpy.fromfile()` with custom `np.dtype` for zero-copy dataframe construction
  * **Expected Outcome**: 80-90% reduction in JSON export cycles, moving telemetry cost from critical path to background

### **2. Branch Reduction in Annual Tick (Target: Eliminate ~438k Mispredictions)**

> * **Problem**: Per-stirpes calculations and lifecycle mutations cause 438,146 conditional branch mispredictions during simulation steps.  
> * **Solution**:  
  * Replace HeirState if/else checks with lookup tables or bitwise selection masks.  
  * Flatten settlement loops into arithmetic predicates to leverage compilers' SIMD auto-vectorization capabilities.

[image1]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABMAAAAWCAYAAAAinad/AAAAzElEQVR4XmNgGAW0AguB+CUQKwDxYSB+hyJLIsgD4o1AvA6IJYD4P6o06QBkAAu6IBBMRhcgBmBzjT8Qq6MLIgNvIF4PxPZIYqpA/BmJDwJiQHwbKocBahggtjtC+R1APBHKbgHiMCgbGWBzLdwgJiQxYSA+hsTHBlYCMR+6IMigq1A2KxCnQcUIgetAHIUsAAojkMa1QNwGxBkMEFeRBeoYiHMFUcCGAbdhMegCxACQYUloYqAwdEcTIwqAYvEHA8RQEN6LKj0KhgQAAK1dJRo+O808AAAAAElFTkSuQmCC>

[image2]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABIAAAAVCAYAAABLy77vAAAA+klEQVR4XuWQzQpBQRTHj2KLsvEEFp5AEVsb3sAjUGQlLyBRdqTkJax4A8qO8gTKRmx9/efeufeemYv7kZ1f/eqe/5w5d2aI/pIMnMAKy9rs25MYfMApjMMifMIuvLI+T8Smgh6SmXf08BNzMje8Q+TitL4Qzd8G+cYaNNAXgtInZ5jlWOkIQIPcww5KR0iO5H4fvbZJwp4eSurk3qjXNuJxm3ooWcIbq6twD0swxXKDOzzrIYiS+fcEyzbwIr9nMMfWjOYVPMGszGoyz1tNEn4tce01q+3NaTiEW9hylhX4oB0csToQ/L3E0AirA2GdaAHLfCEMygP/lBcvRToJM6RB3wAAAABJRU5ErkJggg==>
