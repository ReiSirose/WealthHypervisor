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
  * **Vault Lock**: Core vault capital remains protected; descendants can only extract payouts up to their earned multiplier and current branch cap ceiling.
  * **Deterministic Execution**: Eliminates subjective approvals, manual requests, and family drama—distributions execute strictly based on code policy, state math, and verified metrics.
  * **No Automatic Emergency Coverage**: No loop-hole for excused emergency coverage because it eliminate the prudence from risk assessment.

* **Perpetual Capital Solvency (Infinite Runway)**
  * **Stochastic Stress-Testing**: `MarketEngine` models multi-decade asset growth simulating standard S&P 500 market dynamics (~10% average annual returns) via log-normal drift $e^{r_t}$ with Itô correction.
  * **Zero-Depletion Guarantee**: By enforcing dynamic payout caps against simulated portfolio growth, the engine mathematically guarantees the principal vault will never run out of money across multi-generational horizons.

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

The engine processes annual simulation ticks through four sequential phases orchestrated by `MasterFund`.

```text
========================================================================================
                                MASTERFUND SYSTEM ARCHITECTURE
========================================================================================

                                  +-----------------------+
                                  |      MasterFund       |
                                  | (Engine Orchestrator) |
                                  +-----------+-----------+
                                              |
      +-------------------+-------------------+-------------------+-------------------+
      | (1:1)             | (1:1)             | (1:1)             | (1:1)             |
      v                   v                   v                   v                   v
+------------+     +--------------+    +--------------+    +--------------+    +--------------+
|MarketEngine|     | PolicyEngine |    |LineageRegis. |    |TelemetryLog. |    |AnnualSnapshot|
+------------+     +--------------+    +--------------+    +--------------+    +--------------+
| - PRNG     |     | - cap_pct    |    | - branch_     |    | - history:   |    | (Temporary   |
| - std_dev  |     | - multiplier |    |   arena      |    |   vector<    |    |  DTO Staging)|
| - log_mean |     | - ceiling    |    | - beneficiary|    |   Annual     |    +--------------+
+------------+     +--------------+    |   _arena     |    |   Snapshot>  |           |
                                       | - scratchpad |    +--------------+           | (owns)
                                       +------+-------+                               v
                                              |                         +--------------------------+
                                              | (owns contiguous        | - branch_states: vector  |
                                              |  arenas)                | - heir_states: vector    |
                                              v                         +--------------------------+
                              +-------------------------------+
                              |   Flat Contiguous Memory      |
                              +-------------------------------+
                              | BranchNode  [LCRS Tree Array] |
                              | Beneficiary [64-byte CachePOD]|
                              +-------------------------------+
```


### **Core Components**

| Component | Responsibility | Implementation Details |
| :---- | :---- | :---- |
| **MasterFund** | Global simulation orchestrator. | Coordinates sub-engines and owns MarketFund tracking balances (initial\_aum, current\_aum, current\_year). |
| **MarketEngine** | S\&P 500 drift generator. | Uses a \~2.5 KB PRNG engine applying log-normal drift ![][image1] with Itô correction. |
| **PolicyEngine** | Governance & caps calculator. | Stateless governance enforcing a 3.0x net multiplier, 3.0% global cap ratio, and 1.5x base hybrid spillover ceiling. |
| **LineageRegistry** | Tree settlement & hierarchy. | Maintains contiguous arenas, LCRS tree indices, flat beneficiary PODs, and a 3-phase per-stirpes (![][image2]) settlement engine. |
| **DemographicEngine** | Stochastic lifecycle mutation. | Handles stochastic birth/aging, volatility, and $O(1)$ branch and heir registrations directly in registry arenas. |
| **TelemetryLogger** | State capturing & JSON exporter. | Uses double-buffered staging buffers and zero-allocation std::move passes to format structured JSON logs. |

## **Memory Layout & Cache Alignment**

By favoring flat arrays over class hierarchies, objects avoid 8-byte vptr headers and align cleanly to CPU L1 cache line boundaries.

### **BranchNode Data POD (32 Bytes / Cache Aligned)**

> * Left-Child Sibling-Right (LCRS) index-based tree node eliminating dynamic pointer chasing.  
> * uint32\_t branch\_id (4B) | uint32\_t parent\_index (4B)  
> * double virtual\_share\_percentage (8B)  
> * uint32\_t first\_child\_index (4B) | uint32\_t next\_sibling\_index (4B)  
> * uint16\_t child\_count (2B) | uint16\_t active\_heir\_count (2B) | uint32\_t heir\_start\_index (4B)

### **Beneficiary Data POD (64 Bytes / L1 Cache Line Aligned)**

> * Flat layout allowing SIMD auto-vectorization across contiguous arrays.  
> * uint64\_t id (8B)  
> * uint32\_t branch\_id (4B) | uint16\_t age (2B) | HeirState state (1B) | uint8\_t padding (1B)  
> * double annual\_capital\_contribution (8B)  
> * double last\_approved\_base\_payout (8B) | double last\_approved\_spillover\_payout (8B)

## **Performance Profiling & Hardware Counters**

Baseline profiling reveals the runtime performance, bottlenecks, and execution characteristics of the application based on CPU and time profiler metrics.

### **Time & Cycle Distribution**

| Execution Target | Time (ms) | Time % | CPU Cycles | Cycle % |
| :---- | :---- | :---- | :---- | :---- |
| **TelemetryLogger::export\_to\_json** | **10.10 ms** | **76.5%** | **166.20 M** | **83.3%** |
| **MasterFund::step\_annual\_tick** | 1.70 ms | 12.9% | 21.05 M | 10.6% |
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

## **Planned Optimizations**

### **1\. JSON Export Overhaul (Target: \~80% Cycle Reduction)**

> * **Problem**: TelemetryLogger::export\_to\_json consumes **83.3% of CPU cycles** (166.2M cycles) and 76.5% of total runtime due to standard string stream formatting and synchronous I/O operations.  
> * **Solution**:  
  * Instead of use std::ofstream with << operator, using preallocated file with memory map bypassing slow write() system call
  * With memcpy, writing a binary packed file with telemetry struct is faster than using string operation because of slow internal heap allocation
  * Completely eliminate JSON generation and string formatting from the engine runtime.
> * **Posible Implementation**:
  * **Memory-Mapped Storage**: Preallocate a raw binary file using `mio::mmap_sink`.
  * **Direct POD Copy**: Perform a raw `std::memcpy` of packed `AnnualSnapshot`POD structs into mapped disk memory, achieving $O(1)$ write time without kernel `write()` syscalls.
  * **Python Ingestion**: Process telemetry in Python using `numpy.fromfile()` with custom structured `np.dtype` definitions, allowing instantaneous $O(1)$ zero-copy loading directly into dataframes or vectorized plotting pipelines.

### **2\. Branch Reduction in Annual Tick (Target: Eliminate \~438k Mispredictions)**

> * **Problem**: Per-stirpes calculations and lifecycle mutations cause 438,146 conditional branch mispredictions during simulation steps.  
> * **Solution**:  
  * Replace HeirState if/else checks with lookup tables or bitwise selection masks.  
  * Flatten settlement loops into arithmetic predicates to leverage compilers' SIMD auto-vectorization capabilities.

[image1]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABMAAAAWCAYAAAAinad/AAAAzElEQVR4XmNgGAW0AguB+CUQKwDxYSB+hyJLIsgD4o1AvA6IJYD4P6o06QBkAAu6IBBMRhcgBmBzjT8Qq6MLIgNvIF4PxPZIYqpA/BmJDwJiQHwbKocBahggtjtC+R1APBHKbgHiMCgbGWBzLdwgJiQxYSA+hsTHBlYCMR+6IMigq1A2KxCnQcUIgetAHIUsAAojkMa1QNwGxBkMEFeRBeoYiHMFUcCGAbdhMegCxACQYUloYqAwdEcTIwqAYvEHA8RQEN6LKj0KhgQAAK1dJRo+O808AAAAAElFTkSuQmCC>

[image2]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABIAAAAVCAYAAABLy77vAAAA+klEQVR4XuWQzQpBQRTHj2KLsvEEFp5AEVsb3sAjUGQlLyBRdqTkJax4A8qO8gTKRmx9/efeufeemYv7kZ1f/eqe/5w5d2aI/pIMnMAKy9rs25MYfMApjMMifMIuvLI+T8Smgh6SmXf08BNzMje8Q+TitL4Qzd8G+cYaNNAXgtInZ5jlWOkIQIPcww5KR0iO5H4fvbZJwp4eSurk3qjXNuJxm3ooWcIbq6twD0swxXKDOzzrIYiS+fcEyzbwIr9nMMfWjOYVPMGszGoyz1tNEn4tce01q+3NaTiEW9hylhX4oB0csToQ/L3E0AirA2GdaAHLfCEMygP/lBcvRToJM6RB3wAAAABJRU5ErkJggg==>
