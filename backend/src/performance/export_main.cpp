// Standalone CLI that runs the full multicore benchmark and writes the
// CSVs required as submission evidence under results/benchmarks/. Kept
// separate from nexus_server so the benchmark can be run (and timed)
// without an HTTP server in the loop.
//
// Usage: nexus_performance_export [output_dir]  (default: ../../results/benchmarks)

#include <iomanip>
#include <iostream>
#include "nexus/performance.hpp"

int main(int argc, char** argv) {
    const std::string output_dir = argc > 1 ? argv[1] : "../../results/benchmarks";

    nexus::performance::BenchmarkConfig config;  // defaults: seed 42, 40 cases, N=40 mesh, 5 reps
    std::cout << "Running multicore benchmark (seed=" << config.seed << ", cases=" << config.case_count
              << ", mesh_elements=" << config.mesh_elements << ", repetitions=" << config.repetitions
              << ")...\n";

    const auto report = nexus::performance::run_full_benchmark(config);

    std::cout << "\nHardware:\n";
    std::cout << "  CPU model: " << report.hardware.cpu_model << '\n';
    std::cout << "  Logical cores: " << report.hardware.logical_cores << '\n';
    std::cout << "  Physical cores (estimate" << (report.hardware.physical_cores_is_estimate ? ", = logical on this platform" : "")
              << "): " << report.hardware.physical_cores << '\n';
    std::cout << "  RAM: " << report.hardware.ram_mb << " MB\n";
    std::cout << "  Compiler: " << report.hardware.compiler << '\n';
    std::cout << "  Build type: " << report.hardware.build_type << '\n';
    std::cout << "  Optimization flags: " << report.hardware.optimization_flags << '\n';

    std::cout << "\nWorkers\tMedian (s)\tSpeedup\tEfficiency\n";
    for (const auto& e : report.entries) {
        std::cout << e.num_workers << '\t' << std::fixed << std::setprecision(4) << e.median_seconds << '\t'
                   << e.speedup << '\t' << e.efficiency << '\n';
    }

    std::cout << "\nMax abs serial/parallel agreement error: " << std::scientific
               << report.max_abs_agreement_error_C << " C (threshold: 1e-8 C)\n";
    std::cout << std::defaultfloat;
    std::cout << "Warm-up: " << report.warmup_note << '\n';

    const auto files = nexus::performance::export_benchmark_csv(report, output_dir);
    std::cout << "\nWrote " << files.size() << " CSV file(s) to " << output_dir << ":\n";
    for (const auto& f : files) {
        std::cout << "  " << f << '\n';
    }
    return 0;
}
