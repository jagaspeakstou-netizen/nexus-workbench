#include <gtest/gtest.h>
#include <thread>
#include "nexus/performance.hpp"

using namespace nexus::performance;

TEST(CaseGenerator, IsDeterministicForAFixedSeed) {
    const auto a = generate_cases(/*seed=*/7, 20, 5.0, 50.0, 1000.0, 20000.0);
    const auto b = generate_cases(/*seed=*/7, 20, 5.0, 50.0, 1000.0, 20000.0);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].id, b[i].id);
        EXPECT_DOUBLE_EQ(a[i].k, b[i].k);
        EXPECT_DOUBLE_EQ(a[i].q, b[i].q);
    }
}

TEST(CaseGenerator, DifferentSeedsProduceDifferentCases) {
    const auto a = generate_cases(1, 10, 5.0, 50.0, 1000.0, 20000.0);
    const auto b = generate_cases(2, 10, 5.0, 50.0, 1000.0, 20000.0);
    bool any_different = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].k != b[i].k || a[i].q != b[i].q) any_different = true;
    }
    EXPECT_TRUE(any_different);
}

TEST(CaseGenerator, AllCasesHavePositiveKAndQWithinRequestedRange) {
    const auto cases = generate_cases(123, 50, 5.0, 50.0, 1000.0, 20000.0);
    for (const auto& c : cases) {
        EXPECT_GT(c.k, 0.0);
        EXPECT_GT(c.q, 0.0);
        EXPECT_GE(c.k, 5.0);
        EXPECT_LE(c.k, 50.0);
        EXPECT_GE(c.q, 1000.0);
        EXPECT_LE(c.q, 20000.0);
    }
}

TEST(BatchRun, SerialResultsAreInCaseOrder) {
    const auto cases = generate_cases(1, 8, 5.0, 50.0, 1000.0, 20000.0);
    const auto result = run_batch_serial(cases, /*mesh_elements=*/10);
    ASSERT_EQ(result.case_results.size(), cases.size());
    for (std::size_t i = 0; i < cases.size(); ++i) {
        EXPECT_EQ(result.case_results[i].case_id, cases[i].id);
    }
}

TEST(BatchRun, ParallelResultsAreInCaseOrderRegardlessOfWorkerCount) {
    // This is the "deterministic result ordering" requirement: each
    // worker writes to its own pre-assigned slot (see run_batch_parallel
    // in performance.cpp), so the result vector's order must match the
    // input case order even though threads may finish in any order.
    const auto cases = generate_cases(1, 16, 5.0, 50.0, 1000.0, 20000.0);
    for (int workers : {1, 2, 4}) {
        const auto result = run_batch_parallel(cases, /*mesh_elements=*/10, workers);
        ASSERT_EQ(result.case_results.size(), cases.size());
        for (std::size_t i = 0; i < cases.size(); ++i) {
            EXPECT_EQ(result.case_results[i].case_id, cases[i].id) << "workers=" << workers;
        }
    }
}

TEST(BatchRun, SerialAndParallelAgreeWithinOneETenEightCelsius) {
    // The brief's explicit numerical tolerance for serial/parallel
    // agreement: identical inputs and numerical method must produce
    // the same answer regardless of thread count.
    const auto cases = generate_cases(42, 24, 5.0, 50.0, 1000.0, 20000.0);
    const auto serial = run_batch_serial(cases, 40);
    const auto parallel = run_batch_parallel(cases, 40, 4);
    const double max_diff = max_abs_temperature_difference(serial, parallel);
    EXPECT_LE(max_diff, 1e-8);
}

TEST(BatchRun, ParallelMatchesSerialAtEveryTestedWorkerCount) {
    const auto cases = generate_cases(9, 12, 5.0, 50.0, 1000.0, 20000.0);
    const auto serial = run_batch_serial(cases, 20);
    for (int workers : {1, 2, 3, 4}) {
        const auto parallel = run_batch_parallel(cases, 20, workers);
        EXPECT_LE(max_abs_temperature_difference(serial, parallel), 1e-8) << "workers=" << workers;
    }
}

TEST(Hardware, LogicalCoresIsPositiveAndMatchesStdThread) {
    const auto hw = detect_hardware();
    EXPECT_EQ(hw.logical_cores, static_cast<int>(std::thread::hardware_concurrency()));
    EXPECT_GT(hw.logical_cores, 0);
}

TEST(Benchmark, NeverExceedsAvailableLogicalCpus) {
    BenchmarkConfig config;
    config.case_count = 6;
    config.mesh_elements = 10;
    config.repetitions = 1;
    config.worker_counts = {1, 2, 999};  // 999 should get capped, not crash or hang

    const auto report = run_full_benchmark(config);
    const int logical = report.hardware.logical_cores;
    for (const auto& e : report.entries) {
        EXPECT_LE(e.num_workers, logical);
    }
}

TEST(Benchmark, OneWorkerBaselineHasZeroSpeedupAndMultiWorkerHasPositiveSpeedup) {
    BenchmarkConfig config;
    config.case_count = 20;
    config.mesh_elements = 20;
    config.repetitions = 3;
    config.worker_counts = {1, 2};

    const auto report = run_full_benchmark(config);
    ASSERT_FALSE(report.entries.empty());
    EXPECT_EQ(report.entries.front().num_workers, 1);
    EXPECT_DOUBLE_EQ(report.entries.front().speedup, 0.0);

    if (report.hardware.logical_cores >= 2) {
        ASSERT_GE(report.entries.size(), 2u);
        EXPECT_GT(report.entries[1].speedup, 0.0);
        EXPECT_DOUBLE_EQ(report.entries[1].efficiency, report.entries[1].speedup / 2.0);
    }
}

TEST(Benchmark, ReportsAgreementErrorWithinTolerance) {
    BenchmarkConfig config;
    config.case_count = 10;
    config.mesh_elements = 20;
    config.repetitions = 1;
    config.worker_counts = {1, 2, 4};

    const auto report = run_full_benchmark(config);
    EXPECT_LE(report.max_abs_agreement_error_C, 1e-8);
}
