# MasterFund: High-Performance Wealth Inheritance Hypervisor

A cache-friendly, data-oriented simulation engine for modeling multi-generational inheritance, branch-based governance, and annual settlement logic.

## Table of Contents

- [Overview](#overview)
- [Core Governance](#core-governance)
- [Prerequisites](#prerequisites)
- [Build & Run](#build--run)
- [Architecture Overview](#architecture-overview)
- [Core Components](#core-components)
- [Performance Profiling & Hardware Counters](#performance-profiling--hardware-counters)
- [Technical Specification](#technical-specification)
- [Complete Usage Example](#complete-usage-example)
- [Optimization Review & Next Steps](#optimization-review--next-steps)

## Overview

The Wealth Hypervisor acts as an automated family referee for wealth governance. It enforces algorithmic rules to preserve capital, reward productive contribution, and distribute funds across family branches in a deterministic and policy-driven way.

This project is designed around a data-oriented architecture: flat arenas, cache-aligned PODs, branch-based per-stirpes logic, and minimal heap churn.

## Core Governance

- Contributory earn-out and multiplier payout
  - Beneficiaries can only receive distributions by contributing capital first
  - Payouts are capped by configured multipliers and branch share rules

- Capital protection and no favoritism
  - Annual distributions are capped to a safe fraction of AUM
  - Branch-level and per-heir ceilings prevent over-distribution
  - Settlement logic is deterministic and rule-based rather than discretionary

- Long-term solvency
  - Market growth is modeled stochastically to stress-test multi-decade survivability
  - Policy enforcement ensures the fund remains solvent across long inheritance horizons

## Prerequisites

- C++20 compliant compiler
- CMake 3.20+

## Build & Run

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
./build/fund_sim
```

## Architecture Overview

The engine processes annual simulation ticks through a **3-Phase Settlement Cycle** orchestrated by `MasterFund`, with demographic mutations handled by `DemographicEngine`.

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
| (MT19937)|   | STRICT   |  | Base Cap  |  | Snapshots |  | Deaths|  Payouts  |
| Seed     |   | DYNAMIC   |  | Phase 2:  |  | (move)    |  | Aging |  Branches |
| CAGR     |   | HYBRID    |  | Inherit  |  | JSON Exp. |  | Branch|  Heirs    |
| Volatil. |   | Multipier |  | Phase 3:  |  |           |  | Cretn |          |
+----------+   | Caps      |  | Spillover |  +-----------+  +------+  +---------+
               +-----------+  +-----------+
                                  |
                  ┌───────────────┴────────────────┐
                  │  Flat Contiguous Arenas        │
                  ├────────────────────────────────┤
                  │ BranchNode:  32-byte aligned   │
                  │  - LCRS tree indices (no ptrs) │
                  │  - Per-stirpes share ratios    │
                  │                                │
                  │ Beneficiary: 64-byte aligned  │
                  │  - HeirState (MINOR/ACTIVE)   │
                  │  - Capital contributions      │
                  │  - Payout history             │
                  └────────────────────────────────┘
```

## Core Components

| Component | Responsibility |
| :---- | :---- |
| `MasterFund` | Runs the annual simulation tick and coordinates all sub-systems |
| `MarketEngine` | Models stochastic asset growth and returns |
| `PolicyEngine` | Enforces cap, multiplier, and governance rules |
| `LineageRegistry` | Maintains branches, heirs, and settlement logic |
| `DemographicEngine` | Evolves family structure over time |
| `TelemetryLogger` | Captures snapshots and stores/export telemetry |
| `AnnualSnapshot` | Stores per-year settlement and aggregate state |

## Performance Profiling & Hardware Counters

The main bottleneck in the original profile was telemetry export. Switching from JSON serialization to `export_to_binary_mmap` moved the hot path away from formatting and I/O and into the annual settlement logic. Cycle counts are the most reliable signal because wall-clock measurements are noisy at sub-millisecond latency.

### JSON vs Binary Mmap

| Target | JSON | Binary Mmap | Improvement |
| :---- | :---- | :---- | :---- |
| `TelemetryLogger::export_to_json` / `export_to_binary_mmap` | **10.10 ms** / **166.20 M cycles** | **1.00 ms** / **124.49 k cycles** | **10.1x faster**, **1336x fewer cycles** |
| `MasterFund::step_annual_tick` | 1.70 ms / 21.05 M cycles | 5.00 ms / 5.60 M cycles | **3.8x fewer cycles**; time is noisy |
| `DemographicEngine::step_demographics` | 0.40 ms / 6.85 M cycles | ~1.00 ms / 1.50 M cycles | **4.6x fewer cycles**; time is noisy |
| **Total engine run** | **13.20 ms** / **199.50 M cycles** | **14.00 ms** / **11.11 M cycles** | **17.9x fewer cycles** |

### Before vs After Hot-Path Branch Reduction

| Hot path | Before branch reduction | After branch reduction | Change |
| :---- | :---- | :---- | :---- |
| `LineageRegistry::execute_annual_settlement()` | **5.40 M cycles** | **300.40 k cycles** | **94.4% lower** |
| `MasterFund::step_annual_tick()` | **5.60 M cycles** | **501.33 k cycles** | **91.0% lower** |
| **Total program cycles** | **11.11 M cycles** | **7.39 M cycles** | **33.5% lower** |

### Branch Prediction Improvement

| Metric | Before reduce-branching | After reduce-branching | Improvement |
| :---- | :---- | :---- | :---- |
| Incorrectly predicted conditional branches | **98,592** | **55,872** | **43.3% fewer** |
| Incorrectly predicted branches (total) | **99,940** | **57,144** | **42.8% fewer** |
| Total program cycles | **11.11 M** | **7.39 M** | **33.5% fewer** |

### Benchmark Summary (100-year simulation)

| Build / Export path | Elapsed time | Throughput |
| :---- | :---- | :---- |
| JSON export build | **91.0207 ms** | **1098.65 years/sec** |
| Binary mmap export build | **8.6628 ms** | **11543.62 years/sec** |
| **Improvement** | **10.5x faster** | **10.5x higher throughput** |

### Hardware Counters

- L1D load misses: 92,985
- L1D store misses: 62,069
- L1D TLB misses: 30,359
- Incorrectly predicted conditional branches: 55,872
- SIMD vector arithmetic operations: 154,361
- Total cycles in optimized profile: 7,393,938

## Technical Specification

For implementation details including memory layout, governance modes, per-stirpes logic, telemetry format, and the detailed profiling notes, see [SPEC.md](SPEC.md).

## Complete Usage Example

```cpp
#include "MasterFund.hpp"
#include "DemographicEngine.hpp"

int main() {
    MarketEngine market(0.10, 0.15, 42);

    PolicyEngine policy(PolicyConfig{
        .net_match_multiplier = 3.0,
        .global_cap_ratio = 0.03,
        .spillover_cap_ratio = 1.50,
        .age_of_majority = 21,
        .mode = GovernanceMode::HYBRID_SPILLOVER
    });

    MasterFund fund(50'000'000.0, market, policy);

    DemographicEngine demo_engine(DemographicConfig{
        .initial_branches = 100,
        .initial_heirs = 250,
        .annual_birth_rate = 0.03,
        .annual_branch_rate = 0.05,
        .contribution_volatility = 0.20,
        .seed = 1337
    });

    demo_engine.seed_estate(fund);
    fund.run_simulation(100, demo_engine, "./simulation_telemetry.json");

    std::cout << "Ending AUM: $" << fund.get_current_aum() << "\n";
    return 0;
}
```

## Optimization Review & Next Steps

### Implemented Optimization 1: JSON Export → Binary Mmap

**Target**: roughly **80% cycle reduction** in the telemetry export path.

**Problem**: `TelemetryLogger::export_to_json` dominated the runtime profile, consuming **166.20 M cycles** (**83.3%**) and accounting for **10.10 ms** of total wall time.

**Solution**: switch the hot-path export to a memory-mapped binary format using `mio` and explicit POD serialization instead of formatted text output.

**Result vs expectation**:
- Baseline JSON profile: **199.50 M cycles** total
- Binary mmap profile: **11.11 M cycles** total
- Reduction: **~94.4% fewer cycles** overall
- Runtime benchmark: **91.0207 ms → 8.6628 ms**
- Throughput: **1098.65 → 11543.62 years/sec**

This exceeded the original target and moved the remaining bottleneck into the simulation logic rather than telemetry serialization.

### Implemented Optimization 2: Hot-Path Branch Reduction & Scratch-Buffer Reuse

**Target**: reduce branch misprediction in the annual settlement loop and avoid repeated heap churn from yearly temporary vectors.

**Solution**:
- operate on a compact active-heir set and use cached `branch_index` values
- reuse scratch buffers across ticks
- avoid repeated heap allocation during `execute_annual_settlement_future()`

**Result vs expectation**:
- Original `execute_annual_settlement()` profile: **5.40 M cycles**
- Reduced hot-path profile: **300.40 k cycles**
- Reduction: **94.4% fewer cycles** in the settlement loop
- Annual tick cycles: **5.60 M → 501.33 k**
- Total program cycles: **11.11 M → 7.39 M**
- Incorrectly predicted conditional branches: **98,592 → 55,872**
- Total branch mispredictions: **99,940 → 57,144**

### Next Planned Optimization: Demographic Transition Hot Path

The next candidate is `DemographicEngine::step_demographics()`, especially annual lifecycle and contribution updates.

Likely directions include separate active and inactive heir streams, batch transitions using index arrays, compile-time policy constants, and contiguous mutation patterns while preserving simulation semantics.
