cmake_minimum_required(VERSION 3.20)
project(MasterFundSim CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MASTERFUND_ENABLE_SIMD "Enable compiler SIMD/vectorization passes" ON)
option(MASTERFUND_ENABLE_LOOP_UNROLLING "Enable compiler loop unrolling" ON)
option(MASTERFUND_SHOW_OPTIMIZATION_REPORTS "Report vectorized, missed, and unrolled loops" OFF)
option(MASTERFUND_ENABLE_NATIVE_ARCH "Use host-specific SIMD instructions" OFF)

# Shared source files
set(SIMULATION_SOURCES
    src/main.cpp
    src/MasterFund.cpp
    src/LineageRegistry.cpp
    src/TelemetryLogger.cpp
    src/DemographicEngine.cpp
)

# Shared include directories
set(SIMULATION_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include)

function(configure_performance_options target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        if(MASTERFUND_ENABLE_SIMD)
            target_compile_options(${target} PRIVATE
                $<$<CXX_COMPILER_ID:GNU>:-ftree-vectorize>
                $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fvectorize;-fslp-vectorize>
            )
        endif()

        if(MASTERFUND_ENABLE_LOOP_UNROLLING)
            target_compile_options(${target} PRIVATE -funroll-loops)
        endif()

        if(MASTERFUND_ENABLE_NATIVE_ARCH)
            target_compile_options(${target} PRIVATE -march=native)
        endif()
    elseif(MSVC AND MASTERFUND_ENABLE_SIMD)
        target_compile_options(${target} PRIVATE /arch:AVX2)
    endif()

    if(MASTERFUND_SHOW_OPTIMIZATION_REPORTS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
            target_compile_options(${target} PRIVATE
                -Rpass=loop-vectorize
                -Rpass-missed=loop-vectorize
                -Rpass-analysis=loop-vectorize
                -Rpass=loop-unroll
            )
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE
                -fopt-info-vec-optimized
                -fopt-info-vec-missed
                -fopt-info-loop-optimized
                -fopt-info-loop-missed
            )
        elseif(MSVC)
            message(WARNING "MSVC does not provide equivalent per-loop optimization diagnostics through CMake flags.")
        endif()
    endif()
endfunction()

# ==============================================================================
# Target 1: Production Release Binary (`fund_sim`)
# ==============================================================================
add_executable(fund_sim ${SIMULATION_SOURCES})
target_include_directories(fund_sim PRIVATE ${SIMULATION_INCLUDES})

if(MSVC)
    target_compile_options(fund_sim PRIVATE /O2 /permissive-)
else()
    target_compile_options(fund_sim PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()
configure_performance_options(fund_sim)

# ==============================================================================
# Target 2: Profiling Binary (`fund_sim_profile`)
# ==============================================================================
add_executable(fund_sim_profile ${SIMULATION_SOURCES})
target_include_directories(fund_sim_profile PRIVATE ${SIMULATION_INCLUDES})

if(MSVC)
    target_compile_options(fund_sim_profile PRIVATE /O2 /Zi /Oy-)
else()
    target_compile_options(fund_sim_profile PRIVATE 
        -O2 
        -g 
        -fno-omit-frame-pointer 
        -fno-inline 
        -fno-optimize-sibling-calls 
        -Wall 
        -Wextra
    )
endif()
configure_performance_options(fund_sim_profile)