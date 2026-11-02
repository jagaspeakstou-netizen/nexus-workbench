#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nexus::performance {

// ---------------------------------------------------------------------
// Reproducible batch of independent thermal cases
// ---------------------------------------------------------------------

struct ThermalCase {
    int id = 0;
    double k = 0.0;  // W/(m K)
    double q = 0.0;  // W/m^3
};

// Deterministic (fixed-seed) case generator: the same (seed, count,
// ranges) always produces the same cases, in the same order, so serial
// and parallel runs -- and separate invocations of this CLI/API -- are
// comparable. Each case reuses the thermal module's existing solver
// (nexus::thermal::solve) with the case's own k/q and everything else
// fixed, which is what "independent thermal cases" means here: separate
// FEA solves, not a single bigger coupled problem.
std::vector<ThermalCase> generate_cases(unsigned seed, int count, double k_min, double k_max,
                                         double q_min, double q_max);

// One case's result, reduced to its nodal temperature vector (enough to
// check serial/parallel numerical agreement node-by-node, per the
// brief's 1e-8 C tolerance).
struct CaseResult {
    int case_id = 0;
    std::vector<double> nodal_temperatures;
};

struct BatchRunResult {
    std::vector<CaseResult> case_results;  // always in case-id order, see run_batch_* below
    double wall_time_seconds = 0.0;
};

// Runs every case sequentially on the calling thread. `mesh_elements`
// is the same uniform mesh size used by every case.
BatchRunResult run_batch_serial(const std::vector<ThermalCase>& cases, int mesh_elements);

// Runs the batch across `num_workers` std::thread workers pulling from
// a shared atomic index (a bounded work-stealing pool, not one thread
// per case). Writes each case's result directly into its own
// pre-allocated slot by case index -- NOT via push_back from worker
// threads -- so result ordering is deterministic regardless of which
// thread finishes which case first or in what order.
//
// Throws std::invalid_argument if num_workers < 1. Day 11 validation
// pass: with zero workers, zero threads get spawned, so the pre-sized
// (but default-constructed, i.e. all-zero) result slots would otherwise
// come back completely unpopulated -- silently wrong rather than an
// error. run_full_benchmark() never triggers this itself (it clamps
// worker counts to >= 1 before calling), so this guard exists purely to
// make a direct misuse of this function fail loudly instead of
// returning fabricated-looking zeroed data.
BatchRunResult run_batch_parallel(const std::vector<ThermalCase>& cases, int mesh_elements,
                                   int num_workers);

// Max absolute nodal temperature difference between two batch results
// covering the same cases in the same order. Used to verify serial and
// parallel runs agree (brief requires <= 1e-8 C).
double max_abs_temperature_difference(const BatchRunResult& a, const BatchRunResult& b);

// ---------------------------------------------------------------------
// Hardware / build disclosure (brief: "never fabricate performance
// measurements... report the hardware used")
// ---------------------------------------------------------------------

struct HardwareInfo {
    std::string cpu_model = "unknown";
    int physical_cores = 0;          // best-effort; 0 if undetectable
    int logical_cores = 0;           // std::thread::hardware_concurrency()
    long ram_mb = 0;                 // best-effort; 0 if undetectable
    std::string compiler;            // e.g. "GCC 13.3.0"
    std::string build_type;          // CMAKE_BUILD_TYPE at compile time
    std::string optimization_flags;  // CMAKE_CXX_FLAGS at compile time
    bool physical_cores_is_estimate = false;
};

HardwareInfo detect_hardware();

// ---------------------------------------------------------------------
// Benchmark
// ---------------------------------------------------------------------

struct BenchmarkEntry {
    int num_workers = 0;
    std::vector<double> rep_times_seconds;  // one per measured repetition
    double median_seconds = 0.0;
    double speedup = 0.0;     // T(1 worker) / T(this entry), 0 for the 1-worker baseline itself
    double efficiency = 0.0;  // speedup / num_workers
};

struct BenchmarkConfig {
    unsigned seed = 42;
    // Deliberately large enough that per-case work (microseconds) isn't
    // swamped by thread-launch overhead (also microseconds) -- with too
    // small a workload, "parallel" measures thread creation cost, not
    // the algorithm. See docs/architecture/performance_lab.md for the
    // measured effect of this on this project's own dev container.
    int case_count = 4000;
    int mesh_elements = 200;
    double k_min = 5.0, k_max = 50.0;
    double q_min = 2000.0, q_max = 20000.0;
    int repetitions = 5;
    std::vector<int> worker_counts = {1, 2, 3, 4};  // capped to hardware_concurrency() internally
};

struct BenchmarkReport {
    BenchmarkConfig config;
    HardwareInfo hardware;
    std::vector<BenchmarkEntry> entries;  // one per (capped, deduplicated) worker count, ascending
    double max_abs_agreement_error_C = 0.0;  // serial vs the highest-worker-count parallel run
    std::string warmup_note;
    std::string launch_to_ready_note;
};

// Runs the full benchmark: generates cases once (shared across every
// worker-count configuration, so they're all solving the exact same
// problem), does one untimed warm-up pass per configuration, then times
// `config.repetitions` repetitions and takes the median. Also runs one
// extra (untimed) serial + highest-worker-count parallel pair purely to
// compute the numerical agreement check.
BenchmarkReport run_full_benchmark(const BenchmarkConfig& config);

// Writes per-worker-count timing CSV + a summary CSV under output_dir.
// Returns the list of file paths written.
std::vector<std::string> export_benchmark_csv(const BenchmarkReport& report,
                                                const std::string& output_dir);

}  // namespace nexus::performance
