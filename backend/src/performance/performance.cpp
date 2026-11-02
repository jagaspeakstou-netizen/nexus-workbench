#include "nexus/performance.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "nexus/thermal.hpp"

namespace nexus::performance {

namespace {

CaseResult solve_case(const ThermalCase& c, int mesh_elements) {
    nexus::thermal::ProblemDefinition p;
    p.k = c.k;
    p.q = c.q;
    // L, A, T0, TL stay at the assignment's fixed defaults -- only k and
    // q vary between cases, per the brief ("varied positive thermal
    // conductivity k, varied positive heat generation q").
    const auto solved = nexus::thermal::solve(p, mesh_elements);

    CaseResult result;
    result.case_id = c.id;
    result.nodal_temperatures.reserve(solved.nodes.size());
    for (const auto& n : solved.nodes) {
        result.nodal_temperatures.push_back(n.temperature);
    }
    return result;
}

}  // namespace

std::vector<ThermalCase> generate_cases(unsigned seed, int count, double k_min, double k_max,
                                         double q_min, double q_max) {
    std::vector<ThermalCase> cases;
    cases.reserve(static_cast<std::size_t>(count));

    // std::mt19937 + fixed seed + a fixed draw sequence (k then q, case
    // by case) is what makes this reproducible: the same seed always
    // produces the same cases in the same order, independent of thread
    // count or machine, which is required for serial/parallel runs (and
    // separate benchmark invocations) to be comparable at all.
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> k_dist(k_min, k_max);
    std::uniform_real_distribution<double> q_dist(q_min, q_max);

    for (int i = 0; i < count; ++i) {
        ThermalCase c;
        c.id = i;
        c.k = k_dist(rng);
        c.q = q_dist(rng);
        cases.push_back(c);
    }
    return cases;
}

BatchRunResult run_batch_serial(const std::vector<ThermalCase>& cases, int mesh_elements) {
    BatchRunResult result;
    result.case_results.resize(cases.size());

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < cases.size(); ++i) {
        result.case_results[i] = solve_case(cases[i], mesh_elements);
    }
    const auto end = std::chrono::steady_clock::now();

    result.wall_time_seconds = std::chrono::duration<double>(end - start).count();
    return result;
}

BatchRunResult run_batch_parallel(const std::vector<ThermalCase>& cases, int mesh_elements,
                                   int num_workers) {
    if (num_workers < 1) {
        // Day 11 validation pass: previously, num_workers=0 (or negative)
        // silently spawned zero threads and returned a pre-sized but
        // never-populated (all-default/zero) case_results vector instead
        // of computing anything -- a genuine, unguarded edge case, not a
        // hypothetical one. Fail loudly instead.
        throw std::invalid_argument(
            "run_batch_parallel: num_workers must be >= 1, got " + std::to_string(num_workers));
    }

    BatchRunResult result;
    result.case_results.resize(cases.size());  // pre-sized: each worker writes to its own
                                                 // index, so final ordering is deterministic
                                                 // regardless of which worker finishes first.

    std::atomic<std::size_t> next_index{0};

    auto worker_fn = [&]() {
        while (true) {
            const std::size_t i = next_index.fetch_add(1);
            if (i >= cases.size()) break;
            result.case_results[i] = solve_case(cases[i], mesh_elements);
        }
    };

    const auto start = std::chrono::steady_clock::now();
    {
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(num_workers));
        for (int w = 0; w < num_workers; ++w) {
            workers.emplace_back(worker_fn);
        }
        for (auto& t : workers) t.join();
    }
    const auto end = std::chrono::steady_clock::now();

    result.wall_time_seconds = std::chrono::duration<double>(end - start).count();
    return result;
}

double max_abs_temperature_difference(const BatchRunResult& a, const BatchRunResult& b) {
    double max_diff = 0.0;
    const std::size_t n = std::min(a.case_results.size(), b.case_results.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto& ta = a.case_results[i].nodal_temperatures;
        const auto& tb = b.case_results[i].nodal_temperatures;
        const std::size_t m = std::min(ta.size(), tb.size());
        for (std::size_t j = 0; j < m; ++j) {
            max_diff = std::max(max_diff, std::abs(ta[j] - tb[j]));
        }
    }
    return max_diff;
}

namespace {

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n == 0) return 0.0;
    if (n % 2 == 1) return v[n / 2];
    return (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

}  // namespace

BenchmarkReport run_full_benchmark(const BenchmarkConfig& config) {
    BenchmarkReport report;
    report.config = config;
    report.hardware = detect_hardware();

    const auto cases =
        generate_cases(config.seed, config.case_count, config.k_min, config.k_max, config.q_min, config.q_max);

    // Cap and deduplicate worker counts at the machine's actual logical
    // CPU count -- "never exceed the number of available logical CPUs".
    const int max_workers = std::max(1, report.hardware.logical_cores);
    std::vector<int> worker_counts;
    for (int w : config.worker_counts) {
        const int capped = std::min(w, max_workers);
        if (std::find(worker_counts.begin(), worker_counts.end(), capped) == worker_counts.end()) {
            worker_counts.push_back(capped);
        }
    }
    std::sort(worker_counts.begin(), worker_counts.end());

    double baseline_median = 0.0;  // 1-worker median, used for speedup/efficiency

    for (int w : worker_counts) {
        // Warm-up: one full untimed pass, to absorb first-touch page
        // faults / allocator warm-up / thread creation overhead before
        // the measured repetitions -- otherwise the first measured rep
        // is systematically slower than the rest for reasons that have
        // nothing to do with the algorithm.
        if (w == 1) {
            run_batch_serial(cases, config.mesh_elements);
        } else {
            run_batch_parallel(cases, config.mesh_elements, w);
        }

        BenchmarkEntry entry;
        entry.num_workers = w;
        entry.rep_times_seconds.reserve(static_cast<std::size_t>(config.repetitions));
        for (int r = 0; r < config.repetitions; ++r) {
            const auto run = (w == 1) ? run_batch_serial(cases, config.mesh_elements)
                                       : run_batch_parallel(cases, config.mesh_elements, w);
            entry.rep_times_seconds.push_back(run.wall_time_seconds);
        }
        entry.median_seconds = median(entry.rep_times_seconds);
        if (w == 1) {
            baseline_median = entry.median_seconds;
            entry.speedup = 0.0;
            entry.efficiency = 0.0;
        } else {
            entry.speedup = (entry.median_seconds > 0.0) ? baseline_median / entry.median_seconds : 0.0;
            entry.efficiency = entry.speedup / static_cast<double>(w);
        }
        report.entries.push_back(entry);
    }

    // Agreement check: one untimed serial run vs one untimed run at the
    // highest worker count, compared node-by-node, case-by-case.
    const int max_w = worker_counts.back();
    const auto serial_check = run_batch_serial(cases, config.mesh_elements);
    const auto parallel_check =
        (max_w == 1) ? serial_check : run_batch_parallel(cases, config.mesh_elements, max_w);
    report.max_abs_agreement_error_C = max_abs_temperature_difference(serial_check, parallel_check);

    report.warmup_note =
        "One untimed full batch pass per worker-count configuration, run immediately before "
        "that configuration's " +
        std::to_string(config.repetitions) + " measured repetitions.";
    report.launch_to_ready_note =
        "Not applicable to this CLI/API path (no process launch or UI render is part of what's "
        "timed here -- see docs/architecture/performance_lab.md for what's excluded and why).";

    if (max_workers == 1) {
        report.warmup_note +=
            " NOTE: this machine reports only 1 logical CPU, so the \"parallel\" path runs with "
            "num_workers=1 and speedup cannot be meaningfully assessed here -- see the report for "
            "details.";
    }

    return report;
}

std::vector<std::string> export_benchmark_csv(const BenchmarkReport& report,
                                                const std::string& output_dir) {
    std::filesystem::create_directories(output_dir);
    std::vector<std::string> written;

    {
        std::string path = output_dir + "/performance_timings.csv";
        std::ofstream out(path);
        out << "num_workers,repetition,wall_time_seconds\n";
        for (const auto& e : report.entries) {
            for (std::size_t r = 0; r < e.rep_times_seconds.size(); ++r) {
                out << e.num_workers << ',' << r << ',' << e.rep_times_seconds[r] << '\n';
            }
        }
        written.push_back(path);
    }

    {
        std::string path = output_dir + "/performance_summary.csv";
        std::ofstream out(path);
        out << "num_workers,median_seconds,speedup,efficiency\n";
        for (const auto& e : report.entries) {
            out << e.num_workers << ',' << e.median_seconds << ',' << e.speedup << ',' << e.efficiency << '\n';
        }
        written.push_back(path);
    }

    {
        std::string path = output_dir + "/performance_hardware.csv";
        std::ofstream out(path);
        out << "field,value\n";
        out << "cpu_model,\"" << report.hardware.cpu_model << "\"\n";
        out << "logical_cores," << report.hardware.logical_cores << '\n';
        out << "physical_cores_estimate," << report.hardware.physical_cores << '\n';
        out << "physical_cores_is_estimate," << (report.hardware.physical_cores_is_estimate ? "true" : "false")
            << '\n';
        out << "ram_mb," << report.hardware.ram_mb << '\n';
        out << "compiler,\"" << report.hardware.compiler << "\"\n";
        out << "build_type," << report.hardware.build_type << '\n';
        out << "optimization_flags,\"" << report.hardware.optimization_flags << "\"\n";
        out << "max_abs_agreement_error_C," << report.max_abs_agreement_error_C << '\n';
        out << "case_count," << report.config.case_count << '\n';
        out << "mesh_elements," << report.config.mesh_elements << '\n';
        out << "seed," << report.config.seed << '\n';
        written.push_back(path);
    }

    return written;
}

}  // namespace nexus::performance
