// Small standalone CLI that runs the N=10/20/40/80 thermal mesh
// convergence study and writes the CSVs the brief requires under
// results/csv/. Kept separate from nexus_server so "generate the
// evidence files" doesn't require the HTTP server to be running --
// useful for CI and for the submission checklist.
//
// Usage: nexus_thermal_export [output_dir]   (default: ../results/csv
// relative to the current working directory, i.e. run from backend/build)

#include <iostream>
#include "nexus/thermal.hpp"

int main(int argc, char** argv) {
    const std::string output_dir = argc > 1 ? argv[1] : "../../results/csv";

    nexus::thermal::ProblemDefinition problem;
    const auto results = nexus::thermal::mesh_convergence_study(problem, {10, 20, 40, 80});
    const auto files = nexus::thermal::export_study_csv(problem, results, output_dir);

    std::cout << "Thermal FEA mesh convergence study\n";
    std::cout << "L=" << problem.L << " m, A=" << problem.A << " m^2, k=" << problem.k
              << " W/(m K), q=" << problem.q << " W/m^3, T(0)=" << problem.T0
              << " C, T(L)=" << problem.TL << " C\n\n";
    std::cout << "N\th (m)\tmax |FE_mid - exact| (C)\n";
    for (const auto& r : results) {
        std::cout << r.num_elements << '\t' << r.element_size_h << '\t'
                  << r.max_abs_midpoint_error << '\n';
    }
    std::cout << "\nWrote " << files.size() << " CSV file(s) to " << output_dir << ":\n";
    for (const auto& f : files) {
        std::cout << "  " << f << '\n';
    }
    return 0;
}
