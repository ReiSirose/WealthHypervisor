cmake_minimum_required(VERSION 3.14)
project(EngineUnitTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

enable_testing()

add_executable(unit_test 
        beneficiaries_test.cpp 
        test_lineage.cpp
        ../src/LineageRegistry.cpp) 


target_include_directories(unit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)


target_link_libraries(unit_test PRIVATE GTest::gtest GTest::gtest_main)


include(GoogleTest)
gtest_discover_tests(unit_test)