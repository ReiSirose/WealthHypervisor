# Technical Specification

This document contains the implementation-level details for the wealth governance engine. The main project overview is in [README.md](README.md).

## Memory Layout & Cache Alignment

By favoring flat arrays over class hierarchies, objects avoid 8-byte vptr headers and align cleanly to CPU cache line boundaries. All POD structs are tightly packed with explicit alignment directives.

### LCRS Tree Explanation

Left-Child Sibling-Right (LCRS) is a pointer-free tree representation using indices into a flat arena:

- Each node stores `first_child_index` and `next_sibling_index`
- Eliminates pointer dereferencing; all tree traversal uses array indices
- Keeps traversal cache-friendly and allocation-free

Example family tree: parent → child_0 → child_1 → child_2 becomes contiguous indices without heap allocations.

### BranchNode Data POD (32 Bytes / Cache Aligned)

```cpp
struct alignas(32) BranchNode {
    uint32_t branch_id;
    uint32_t parent_index;
    double virtual_share_percentage;
    uint32_t first_child_index;
    uint32_t next_sibling_index;
    uint16_t child_count;
    uint16_t active_heir_count;
    uint32_t heir_start_index;
};
```

### Beneficiary Data POD (64 Bytes / L1 Cache Line Aligned)

```cpp
struct alignas(64) Beneficiary {
    uint64_t id;
    double annual_capital_contribution;
    double last_approved_base_payout;
    double last_approved_spillover_payout;
    uint32_t branch_id;
    uint32_t branch_index;
    uint16_t age;
    HeirState state;
    uint8_t padding;
};
```

### HeirState Enumeration

```cpp
enum class HeirState : uint8_t {
    MINOR    = 0,
    INACTIVE = 1,
    ACTIVE   = 2,
    DECEASED = 3
};
```

State transitions:

- `MINOR` → `INACTIVE` on the 21st birthday
- `INACTIVE` → `ACTIVE` when external capital is contributed
- `ACTIVE` → `INACTIVE` if contribution falls to zero
- Any state → `DECEASED` when a death event triggers rebalance

### Struct Size Reference

| Struct | Size | Purpose |
| :---- | :---- | :---- |
| `Beneficiary` | 64 bytes | Cache-line aligned POD |
| `BranchNode` | 32 bytes | LCRS tree node |
| `HeirSnapshot` | 56 bytes | Annual heir telemetry |
| `BranchSnapshot` | 48 bytes | Annual branch telemetry |
| `AnnualSnapshot` | 112 bytes | Complete year snapshot |
| `PolicyConfig` | 32 bytes | Governance rules |
| `MarketEngine` | 2560 bytes | MT19937 PRNG engine |
| `LineageRegistry` | 56 bytes | Two vector pointers + scalar |
| `MasterFund` | 2696 bytes | Aggregates all engines |

## Governance Modes & Policy Enforcement

### PolicyEngine Governance Modes

The `PolicyEngine` supports three strategies:

#### 1. `STRICT_PARTITION`

Each branch receives a fixed entitlement cap based on per-stirpes share:

```text
Branch Ceiling = Global Cap × Branch Share (no spillover allowed)
```

- Predictable and hard-capped
- Unused capital cannot flow to other branches

#### 2. `DYNAMIC_POOLING`

All branches compete for the global cap without hard ceilings:

```text
Branch Ceiling = Global Cap
```

- More aggressive capital deployment
- Can concentrate allocation in a single branch when demand is strong

#### 3. `HYBRID_SPILLOVER` (recommended default)

Each branch gets a base entitlement plus capped spillover allowance:

```text
Branch Ceiling = (Global Cap × Branch Share) × Spillover Cap Ratio
```

Example:

```text
$300k global cap × 0.50 share × 1.5 spillover = $225k effective ceiling
```

This balances fairness and capital efficiency.

### Per-Stirpes Rebalancing

Per-stirpes ensures equitable wealth distribution across family lines. The engine implements this via dynamic share rebalancing:

1. Initial share assignment from branch depth and tree structure
2. Annual rebalance after demographic changes
3. Share recomputation from root to leaves
4. Settlement based on the updated branch shares

This prevents a branch with no active heirs from starving siblings with active demand.

### 3-Phase Settlement Algorithm

Each annual tick executes a deterministic 3-phase settlement cycle. The global cap is:

```text
Global Cap = Current AUM × 3.0%
```

#### Phase 1: Base Entitlement

```text
For each ACTIVE beneficiary:
  raw_demand = annual_capital_contribution × 3.0
  branch_share = branch.virtual_share_percentage
  base_allocation = global_cap × branch_share
  heir_cap = base_allocation / branch.active_heir_count
  base_payout = MIN(raw_demand, heir_cap)
```

This guarantees a fair per-stirpes share without exceeding the branch cap.

#### Phase 2: Inheritance Queue

Reserved for deceased-heir redistribution. This is currently a placeholder for future expansion.

#### Phase 3: Spillover Distribution

```text
Remaining_surplus = global_cap - cash_disbursed_phase1
For each branch with remaining demand:
  spillover_ceiling = (base_allocation × spillover_cap_ratio) - base_payout
  additional_demand = MAX(0, raw_demand - base_payout)
  spillover_payout = MIN(additional_demand, spillover_ceiling, remaining_surplus)
```

This ensures unused capital is redistributed without breaching the global cap.

## MarketEngine: Stochastic Asset Growth Simulation

The `MarketEngine` uses a Mersenne Twister PRNG to simulate realistic market returns via a log-normal distribution.

### Configuration Parameters

```cpp
struct MarketEngine {
    double target_cagr;
    double std_dev;
    uint32_t seed;
    std::mt19937 rng;
    std::normal_distribution<> dist;
};
```

### Annual Growth Calculation

Each year generates a market multiplier via log-normal drift with Itô correction:

$$\text{logMean} = \ln(1 + \text{CAGR}) - \frac{\sigma^2}{2}$$

$$\text{annualMultiplier} = e^{N(\text{logMean}, \sigma)}$$

$$\text{newAUM} = \text{currentAUM} \cdot \text{annualMultiplier}$$

This ensures:

- long-term drift toward the configured CAGR
- positive portfolio values over time
- realistic volatility without negative asset values

### Example Setup

```cpp
MarketEngine market(0.10, 0.15, 42);
market.set_seed(42);
```

## DemographicEngine: Family Growth & Lifecycle

The `DemographicEngine` drives annual family evolution, mutating beneficiary states and branch topology.

### Configuration Parameters

```cpp
struct DemographicConfig {
    size_t initial_branches;
    size_t initial_heirs;
    double annual_birth_rate;
    double annual_branch_rate;
    double contribution_volatility;
    uint32_t seed;
};
```

### Lifecycle Mutations

#### Annual Aging

- Each beneficiary age increments by 1
- On 21st birthday: `MINOR` → `INACTIVE`
- On death: any state → `DECEASED`

#### Birth Events

- A new heir is created with a probability defined by `annual_birth_rate`
- New heirs begin in `MINOR` state

#### Branch Creation

- New sub-branches may split from existing branches
- New branch shares are recalculated during rebalance

#### Contribution Volatility

- Each active heir contribution changes based on random volatility
- Models salary variation and changing family cash flow

### Seeding Initial Estate

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
demo_engine.seed_estate(fund);
```

## Telemetry & Snapshot Capture

After each annual tick, `MasterFund::step_annual_tick()` assembles a `AnnualSnapshot` containing year-level, branch-level, and heir-level state.

### Annual Snapshot Contents

**Year-level aggregates**

- year
- starting_aum / ending_aum
- annual market return
- global_cap_dollars
- total_base_disbursed
- total_spillover_disbursed
- unused_surplus_retained

**Branch snapshot fields**

- branch_id
- virtual_share_percentage
- base_cap_dollars
- base_disbursed
- spillover_disbursed
- active_heir_count

**Heir snapshot fields**

- heir_id
- branch_id
- age
- state
- capital_contribution
- raw_match_demand
- base_payout
- spillover_payout
- unmet_demand

### Export & Logging

`TelemetryLogger` stores annual snapshots in memory and offers multiple export pathways:

1. JSON export for human inspection
2. Binary memory-mapped export using `mio` for performance
3. In-memory access for downstream analytics

The binary format is explicit and portable, rather than relying on raw STL layout assumptions.

## Implementation Notes

- The engine favors flat contiguous storage over dynamic object graphs
- The LUCRS and arena-based design is intended to improve locality and reduce pointer chasing
- Performance wins are primarily from branch reduction, cache locality, and lower allocation churn
- Compiled-time constants and policy configuration are useful, but the dominant gains still come from data-oriented design
