#include "nexus/thermal.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace nexus::thermal {

namespace {

// Solves a tridiagonal system A*x = d where A has sub-diagonal `a`
// (a[0] unused), diagonal `b`, super-diagonal `c` (c[n-1] unused).
// Thomas algorithm: O(n), exact for a well-conditioned tridiagonal
// system such as ours (strictly diagonally dominant after BC
// elimination). Chosen over a general dense solver because the
// assembled FE matrix for 2-node 1D elements is tridiagonal by
// construction -- using a full O(n^3) solver here would be solving
// the wrong-shaped problem.
std::vector<double> thomas_solve(std::vector<double> a, std::vector<double> b,
                                  std::vector<double> c, std::vector<double> d) {
    const std::size_t n = b.size();
    for (std::size_t i = 1; i < n; ++i) {
        const double m = a[i] / b[i - 1];
        b[i] -= m * c[i - 1];
        d[i] -= m * d[i - 1];
    }
    std::vector<double> x(n);
    x[n - 1] = d[n - 1] / b[n - 1];
    for (std::size_t i = n - 1; i-- > 0;) {
        x[i] = (d[i] - c[i] * x[i + 1]) / b[i];
    }
    return x;
}

}  // namespace

double exact_temperature(const ProblemDefinition& p, double x) {
    return p.T0 + (p.TL - p.T0) / p.L * x + (p.q / (2.0 * p.k)) * x * (p.L - x);
}

SolveResult solve(const ProblemDefinition& p, int num_elements) {
    if (num_elements < 1) {
        throw std::invalid_argument("num_elements must be >= 1");
    }

    const int n_nodes = num_elements + 1;
    const double h = p.L / static_cast<double>(num_elements);
    const double ke_diag = p.k * p.A / h;     // kA/h
    // AI CHALLENGE TEST (review/ai-challenge-thermal-fea branch, temporary):
    // deliberately applying the AI-proposed-and-rejected "qAh per node"
    // formulation here to confirm the real GoogleTest suite actually
    // catches it. See docs/technical_report/ai_challenge/ for the full
    // writeup. This commit is reverted by the very next commit on this
    // branch -- it never reaches main.
    const double fe_term = p.q * p.A * h;  // CHALLENGE: qAh (full), not qAh/2

    // --- Assembly ---
    // Global stiffness is tridiagonal: diagonal[i] accumulates ke_diag from
    // each adjacent element (2x for interior nodes, 1x for the two ends);
    // off-diagonals are -ke_diag. Global load: each interior node receives
    // fe_term from both the element to its left and to its right (2*fe_term);
    // end nodes receive it from one element only.
    std::vector<double> sub(n_nodes, 0.0), diag(n_nodes, 0.0), sup(n_nodes, 0.0);
    std::vector<double> load(n_nodes, 0.0);

    for (int e = 0; e < num_elements; ++e) {
        const int i = e;
        const int j = e + 1;
        diag[i] += ke_diag;
        diag[j] += ke_diag;
        sup[i] += -ke_diag;   // K[i][j]
        sub[j] += -ke_diag;   // K[j][i]
        load[i] += fe_term;
        load[j] += fe_term;
    }

    // --- Dirichlet BC enforcement (symmetric elimination) ---
    // For each fixed dof i with value v: subtract K[:,i]*v from the load
    // on every other row, then zero row/column i and put 1 on the
    // diagonal with load[i] = v. For a tridiagonal matrix, column i only
    // touches rows i-1, i, i+1, so this is cheap and exact (no penalty
    // approximation).
    auto apply_dirichlet = [&](int i, double v) {
        if (i - 1 >= 0) {
            load[i - 1] -= sup[i - 1] * v;  // K[i-1][i] == sup[i-1]
            sup[i - 1] = 0.0;
        }
        if (i + 1 < n_nodes) {
            load[i + 1] -= sub[i + 1] * v;  // K[i+1][i] == sub[i+1]
            sub[i + 1] = 0.0;
        }
        sub[i] = 0.0;
        sup[i] = 0.0;
        diag[i] = 1.0;
        load[i] = v;
    };
    apply_dirichlet(0, p.T0);
    apply_dirichlet(n_nodes - 1, p.TL);

    const std::vector<double> temperatures = thomas_solve(sub, diag, sup, load);

    SolveResult result;
    result.num_elements = num_elements;
    result.element_size_h = h;
    result.nodes.reserve(n_nodes);
    for (int i = 0; i < n_nodes; ++i) {
        const double x = i * h;
        result.nodes.push_back({i, x, temperatures[static_cast<std::size_t>(i)]});
    }

    result.midpoints.reserve(num_elements);
    double max_err = 0.0;
    for (int e = 0; e < num_elements; ++e) {
        const double x_mid = (result.nodes[e].x + result.nodes[e + 1].x) / 2.0;
        // Linear FE interpolation at the midpoint == average of the two
        // nodal values (shape functions are both 0.5 there).
        const double fe_t =
            (result.nodes[e].temperature + result.nodes[e + 1].temperature) / 2.0;
        const double exact_t = exact_temperature(p, x_mid);
        const double err = std::abs(fe_t - exact_t);
        max_err = std::max(max_err, err);
        result.midpoints.push_back({e, x_mid, fe_t, exact_t, err});
    }
    result.max_abs_midpoint_error = max_err;

    const auto [min_it, max_it] = std::minmax_element(
        result.nodes.begin(), result.nodes.end(),
        [](const NodeResult& a, const NodeResult& b) { return a.temperature < b.temperature; });
    result.min_temperature = min_it->temperature;
    result.max_temperature = max_it->temperature;

    return result;
}

std::vector<SolveResult> mesh_convergence_study(const ProblemDefinition& p,
                                                  const std::vector<int>& mesh_sizes) {
    std::vector<SolveResult> results;
    results.reserve(mesh_sizes.size());
    for (int n : mesh_sizes) {
        results.push_back(solve(p, n));
    }
    return results;
}

std::vector<std::string> export_study_csv(const ProblemDefinition& p,
                                           const std::vector<SolveResult>& results,
                                           const std::string& output_dir) {
    std::filesystem::create_directories(output_dir);
    std::vector<std::string> written;

    for (const auto& r : results) {
        std::ostringstream fname;
        fname << output_dir << "/thermal_n" << r.num_elements << ".csv";
        std::ofstream out(fname.str());
        out << "kind,index,x_m,temperature_fe_C,temperature_exact_C,abs_error_C\n";
        for (const auto& node : r.nodes) {
            out << "node," << node.index << ',' << node.x << ',' << node.temperature << ','
                << exact_temperature(p, node.x) << ',' << std::abs(node.temperature - exact_temperature(p, node.x))
                << '\n';
        }
        for (const auto& mp : r.midpoints) {
            out << "midpoint," << mp.element_index << ',' << mp.x << ',' << mp.fe_temperature << ','
                << mp.exact_temperature << ',' << mp.abs_error << '\n';
        }
        written.push_back(fname.str());
    }

    {
        std::string conv_path = output_dir + "/thermal_convergence.csv";
        std::ofstream out(conv_path);
        out << "num_elements,element_size_h_m,max_abs_midpoint_error_C\n";
        for (const auto& r : results) {
            out << r.num_elements << ',' << r.element_size_h << ',' << r.max_abs_midpoint_error << '\n';
        }
        written.push_back(conv_path);
    }

    return written;
}

}  // namespace nexus::thermal
