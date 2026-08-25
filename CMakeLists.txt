cmake_minimum_required(VERSION 3.20)
project(MasterFundSim CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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