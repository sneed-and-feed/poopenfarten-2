/**
 * @file pf02_headless_dsp_tests.cpp
 * @brief Headless C++20 DSP Verification Runner for Poopenfarten II (PF-02)
 *
 * Executes Tiers 1-4 Verification Suites:
 *  - Tier 1: Functional & Algorithmic Feature Coverage
 *  - Tier 2: Boundary, Multirate & Buffer Size Resilience
 *  - Tier 3: DarkSnail & MR-16 Mathematical Invariants
 *  - Tier 4: Real-World Scenarios, Zero-Allocation & Stress Hardening
 */

#include <iostream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <string>
#include <cstdlib>

// ============================================================================
// Global Heap Allocation Tracker for Real-Time Audio-Thread Safety Verification
// Must precede other includes
// ============================================================================
bool gTrackAllocations = false;
size_t gAllocationCount = 0;
size_t gBytesAllocated = 0;

void* operator new(size_t size) {
    if (gTrackAllocations) {
        ++gAllocationCount;
        gBytesAllocated += size;
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

void* operator new[](size_t size) {
    if (gTrackAllocations) {
        ++gAllocationCount;
        gBytesAllocated += size;
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p, size_t) noexcept {
    std::free(p);
}

#include "TestHarness.h"
#include "Tier1_FeatureTests.h"
#include "Tier2_BoundaryTests.h"
#include "Tier3_MathematicalInvariantTests.h"
#include "Tier4_StressTests.h"

int main(int argc, char* argv[]) {
    std::cout << "================================================================================\n";
    std::cout << "   POOPENFARTEN II (PF-02) — HEADLESS C++20 DSP VERIFICATION TEST RUNNER       \n";
    std::cout << "        Darm-Akustischer & Ultrametrischer Triaden-Synthesizer                  \n";
    std::cout << "================================================================================\n\n";

    // Parse target tier filter from arguments (default: run all tiers)
    std::string tierFilter = "all";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--tier=1") tierFilter = "Tier 1";
        else if (arg == "--tier=2") tierFilter = "Tier 2";
        else if (arg == "--tier=3") tierFilter = "Tier 3";
        else if (arg == "--tier=4") tierFilter = "Tier 4";
        else if (arg == "--all") tierFilter = "all";
    }

    // Register all test suites across Tiers 1-4
    test::registerTier1Tests();
    test::registerTier2Tests();
    test::registerTier3Tests();
    test::registerTier4Tests();

    auto& registry = test::getTestRegistry();
    const size_t totalAvailable = registry.size();

    std::vector<test::TestCase> testsToRun;
    for (const auto& tc : registry) {
        if (tierFilter == "all" || tc.tier == tierFilter) {
            testsToRun.push_back(tc);
        }
    }

    std::cout << "Filter Active: " << tierFilter << "\n";
    std::cout << "Registered Test Cases to Execute: " << testsToRun.size() << " (out of " << totalAvailable << ")\n";
    std::cout << "--------------------------------------------------------------------------------\n\n";

    int tier1Total = 0, tier1Pass = 0;
    int tier2Total = 0, tier2Pass = 0;
    int tier3Total = 0, tier3Pass = 0;
    int tier4Total = 0, tier4Pass = 0;
    int overallPass = 0;
    int overallFail = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < testsToRun.size(); ++i) {
        const auto& testCase = testsToRun[i];
        test::gCurrentTestAssertFailures = 0;

        auto t0 = std::chrono::high_resolution_clock::now();
        bool success = false;
        try {
            success = testCase.run();
        } catch (const std::exception& e) {
            std::cerr << "      EXCEPTION: " << e.what() << "\n";
            success = false;
        } catch (...) {
            std::cerr << "      UNKNOWN EXCEPTION OCCURRED\n";
            success = false;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();

        bool passed = success && (test::gCurrentTestAssertFailures == 0);

        if (testCase.tier == "Tier 1") {
            ++tier1Total;
            if (passed) ++tier1Pass;
        } else if (testCase.tier == "Tier 2") {
            ++tier2Total;
            if (passed) ++tier2Pass;
        } else if (testCase.tier == "Tier 3") {
            ++tier3Total;
            if (passed) ++tier3Pass;
        } else if (testCase.tier == "Tier 4") {
            ++tier4Total;
            if (passed) ++tier4Pass;
        }

        if (passed) {
            ++overallPass;
            std::cout << "  [" << std::setw(2) << (i + 1) << "/" << testsToRun.size() << "] "
                      << "[PASS] [" << testCase.tier << "] " << testCase.id << ": " << testCase.name
                      << " (" << std::fixed << std::setprecision(1) << elapsedUs << " us)\n";
        } else {
            ++overallFail;
            std::cerr << "  [" << std::setw(2) << (i + 1) << "/" << testsToRun.size() << "] "
                      << "[FAIL] [" << testCase.tier << "] " << testCase.id << ": " << testCase.name << "\n";
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    std::cout << "\n================================================================================\n";
    std::cout << "                  PF-02 DSP VERIFICATION SUITE SUMMARY                          \n";
    std::cout << "================================================================================\n";
    if (tier1Total > 0) {
        std::cout << "  Tier 1 (Feature Coverage)     : " << tier1Pass << " / " << tier1Total
                  << " (" << (tier1Pass * 100 / tier1Total) << "%)\n";
    }
    if (tier2Total > 0) {
        std::cout << "  Tier 2 (Boundary & Corners)   : " << tier2Pass << " / " << tier2Total
                  << " (" << (tier2Pass * 100 / tier2Total) << "%)\n";
    }
    if (tier3Total > 0) {
        std::cout << "  Tier 3 (Math Invariants)      : " << tier3Pass << " / " << tier3Total
                  << " (" << (tier3Pass * 100 / tier3Total) << "%)\n";
    }
    if (tier4Total > 0) {
        std::cout << "  Tier 4 (Stress & Hardening)   : " << tier4Pass << " / " << tier4Total
                  << " (" << (tier4Pass * 100 / tier4Total) << "%)\n";
    }
    std::cout << "--------------------------------------------------------------------------------\n";
    std::cout << "  Total Test Cases Executed     : " << testsToRun.size() << "\n";
    std::cout << "  Passed                        : " << overallPass << "\n";
    std::cout << "  Failed                        : " << overallFail << "\n";
    std::cout << "  Total Suite Execution Time    : " << std::fixed << std::setprecision(2) << totalElapsedMs << " ms\n";
    std::cout << "================================================================================\n";

    const bool allPassed = (overallFail == 0) && (!testsToRun.empty());
    std::cout << "OVERALL E2E VERDICT: " << (allPassed ? "PASS (100% SUCCESS)" : "FAIL") << "\n";
    std::cout << "================================================================================\n";

    return allPassed ? 0 : 1;
}
