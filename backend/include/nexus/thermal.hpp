#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nexus::thermal {

// Problem definition, fixed by the assignment brief (Task C / Module C).
// Governing equation: -d/dx(kA dT/dx) = qA, 0 <= x <= L
// BCs: T(0) = T0, T(L) = TL
struct ProblemDefinition {
    double L = 1.0;        // length, m
    double A = 1e-4;       // cross-sectional area, m^2
    double k = 10.0;       // thermal conductivity, W/(m K)
    double q = 10000.0;    // uniform volumetric heat generation, W/m^3
    double T0 = 100.0;     // Dirichlet BC at x = 0, deg C
    double TL = 20.0;      // Dirichlet BC at x = L, deg C
};

// Exact analytical solution:
//   T(x) = T0 + (TL - T0)/L * x + [q/(2k)] * x * (L - x)
// (reduces to the brief's T_exact(x) = 100 - 80x/L + [q/(2k)]x(L-x) for the
// default ProblemDefinition above). Exposed separately so tests can check
// the FE solution against it independently of solve().
double exact_temperature(const ProblemDefinition& p, double x);

// One mesh node's result.
struct NodeResult {
    int index;
    double x;
    double temperature;
};

// One element's midpoint result. The brief explicitly requires the
// midpoint error to come from FE interpolation (average of the two
// adjacent nodal temperatures for a 2-node linear element), not a
// nodal-only comparison -- that distinction is why this struct exists
// separately from NodeResult.
struct MidpointResult {
    int element_index;
    double x;
    double fe_temperature;      // linear FE interpolation at the midpoint
    double exact_temperature;   // analytical solution at the same x
    double abs_error;
};

// Full result of solving the FEA problem on a uniform mesh of `num_elements`
// two-node linear elements.
struct SolveResult {
    int num_elements = 0;
    double element_size_h = 0.0;
    std::vector<NodeResult> nodes;
    std::vector<MidpointResult> midpoints;
    double max_abs_midpoint_error = 0.0;
    double min_temperature = 0.0;
    double max_temperature = 0.0;
};

// Assembles and solves the steady-state 1D thermal FEA problem with
// `num_elements` uniform two-node linear elements. Throws
// std::invalid_argument if num_elements < 1.
//
// Implementation notes (see thermal.cpp for the full derivation):
//  - Element stiffness: Ke = (kA/h) * [[1,-1],[-1,1]]
//  - Element load (consistent, constant source q): fe = qAh/2 * [1,1]
//  - Global system is tridiagonal; solved directly with the Thomas
//    algorithm (no need for a general dense/sparse solver here).
//  - Dirichlet BCs enforced by symmetric elimination (not penalty), so
//    the assembled system stays interpretable for debugging.
SolveResult solve(const ProblemDefinition& p, int num_elements);

// Runs solve() for several mesh sizes and returns one SolveResult per
// entry in `mesh_sizes`, in the same order. Used for the N=10/20/40/80
// convergence study the brief asks for.
std::vector<SolveResult> mesh_convergence_study(
    const ProblemDefinition& p, const std::vector<int>& mesh_sizes);

// Writes one CSV per mesh size (thermal_n<N>.csv: node/midpoint data) plus
// a convergence summary CSV (thermal_convergence.csv), all under
// `output_dir`. Returns the list of file paths written. This is the
// "exportable CSV containing underlying results" the brief requires.
std::vector<std::string> export_study_csv(
    const ProblemDefinition& p,
    const std::vector<SolveResult>& results,
    const std::string& output_dir);

}  // namespace nexus::thermal
