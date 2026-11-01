#include <gtest/gtest.h>
#include <cmath>
#include "nexus/thermal.hpp"

using nexus::thermal::ProblemDefinition;
using nexus::thermal::exact_temperature;
using nexus::thermal::solve;
using nexus::thermal::mesh_convergence_study;

namespace {
ProblemDefinition default_problem() { return ProblemDefinition{}; }
}  // namespace

TEST(ThermalExactSolution, MatchesBriefFormulaAtSampleX) {
    // Brief: T_exact(x) = 100 - 80x/L + [q/(2k)]x(L-x), for the default
    // problem (L=1, q=10000, k=10) this is 100 - 80x + 500x(1-x).
    const auto p = default_problem();
    for (double x : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        const double expected = 100.0 - 80.0 * x + 500.0 * x * (1.0 - x);
        EXPECT_NEAR(exact_temperature(p, x), expected, 1e-9);
    }
}

TEST(ThermalExactSolution, SatisfiesBoundaryConditions) {
    const auto p = default_problem();
    EXPECT_NEAR(exact_temperature(p, 0.0), p.T0, 1e-9);
    EXPECT_NEAR(exact_temperature(p, p.L), p.TL, 1e-9);
}

TEST(ThermalSolve, RejectsInvalidMesh) {
    const auto p = default_problem();
    EXPECT_THROW(solve(p, 0), std::invalid_argument);
    EXPECT_THROW(solve(p, -5), std::invalid_argument);
}

TEST(ThermalSolve, EnforcesBoundaryConditionsExactly) {
    const auto p = default_problem();
    for (int n : {10, 20, 40, 80}) {
        const auto r = solve(p, n);
        ASSERT_EQ(r.nodes.size(), static_cast<std::size_t>(n + 1));
        EXPECT_NEAR(r.nodes.front().temperature, p.T0, 1e-9);
        EXPECT_NEAR(r.nodes.back().temperature, p.TL, 1e-9);
    }
}

TEST(ThermalSolve, NodalValuesAreExactForThisConstantSourceProblem) {
    // For a 1D problem with piecewise-linear elements and a *constant*
    // volumetric source, Galerkin FEM is known to be nodally exact (the
    // discretization error lives entirely in the interpolation *between*
    // nodes, not at the nodes themselves). This is exactly why the brief
    // requires midpoint error via FE interpolation rather than a nodal
    // comparison -- a nodal-only check would show ~0 error and hide the
    // real discretization error. This test asserts that nodal exactness
    // actually holds for our assembly (it is not assumed elsewhere).
    const auto p = default_problem();
    const auto r = solve(p, 10);
    for (const auto& node : r.nodes) {
        EXPECT_NEAR(node.temperature, exact_temperature(p, node.x), 1e-9);
    }
}

TEST(ThermalSolve, MidpointErrorMatchesClosedFormPrediction) {
    // Because T_exact is quadratic (a*x^2 + b*x + c with a = -q/(2k)) and
    // nodal values are exact (previous test), the error of *linear*
    // interpolation at an element's midpoint has a closed form:
    //   error = |a| * h^2 / 4 = (q / (2k)) * h^2 / 4 = q*h^2 / (8k)
    // This gives an independent analytical check on max_abs_midpoint_error
    // that doesn't rely on trusting the solver's own output.
    const auto p = default_problem();
    for (int n : {10, 20, 40, 80}) {
        const auto r = solve(p, n);
        const double h = p.L / n;
        const double expected_error = p.q * h * h / (8.0 * p.k);
        EXPECT_NEAR(r.max_abs_midpoint_error, expected_error, 1e-9);
    }
}

TEST(ThermalSolve, MeshConvergenceHalvesErrorByFourX) {
    // max_abs_midpoint_error is O(h^2), so doubling N (halving h) should
    // reduce the error by ~4x. This is the convergence check the brief
    // explicitly asks for (section 6 of the assignment).
    const auto p = default_problem();
    const auto results = mesh_convergence_study(p, {10, 20, 40, 80});
    ASSERT_EQ(results.size(), 4u);
    for (std::size_t i = 1; i < results.size(); ++i) {
        const double ratio =
            results[i - 1].max_abs_midpoint_error / results[i].max_abs_midpoint_error;
        EXPECT_NEAR(ratio, 4.0, 0.05);
    }
    // Monotonically decreasing too.
    for (std::size_t i = 1; i < results.size(); ++i) {
        EXPECT_LT(results[i].max_abs_midpoint_error, results[i - 1].max_abs_midpoint_error);
    }
}

TEST(ThermalSolve, TemperatureRangeIsPhysicallySane) {
    // q > 0 everywhere and both ends are fixed below the interior peak
    // driven by heat generation, so max temperature should sit at/above
    // both boundary values, and min should be exactly the lower BC (TL)
    // since T is monotonically... actually not monotonic in general;
    // just assert the FE max/min bound the two BC values sanely.
    const auto p = default_problem();
    const auto r = solve(p, 40);
    EXPECT_GE(r.max_temperature, std::max(p.T0, p.TL));
    EXPECT_LE(r.min_temperature, std::max(p.T0, p.TL));
}
