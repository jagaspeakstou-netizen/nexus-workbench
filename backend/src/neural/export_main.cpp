// nexus_neural_export <output_dir>
//
// Runs the backprop training loop (Module F) and writes two CSVs for
// submission evidence: the per-step training trace and the
// analytic-vs-finite-difference gradient check.

#include <cstdio>
#include <string>

#include "nexus/neural.hpp"

int main(int argc, char** argv) {
    std::string output_dir = (argc > 1) ? argv[1] : ".";

    nexus::neural::ProblemDefinition problem;  // defaults from the brief's example network
    auto result = nexus::neural::train(problem);

    const std::string training_path = output_dir + "/neural_training.csv";
    const std::string gradcheck_path = output_dir + "/neural_gradient_check.csv";

    nexus::neural::export_training_csv(result, training_path);
    nexus::neural::export_gradient_check_csv(result.gradient_check, gradcheck_path);

    std::printf("Wrote %s (%zu steps)\n", training_path.c_str(), result.history.size());
    std::printf("Wrote %s\n", gradcheck_path.c_str());
    std::printf("Initial loss: %.6f  Final loss: %.10g\n", result.initial_loss, result.final_loss);
    std::printf("Gradient check (eps=%.0e, tol=%.0e): %s\n",
                 result.gradient_check.epsilon, result.gradient_check.tolerance,
                 result.gradient_check.all_within_tolerance ? "ALL WITHIN TOLERANCE" : "FAILED");
    for (const auto& e : result.gradient_check.entries) {
        std::printf("  %s: analytic=%.10f finite_diff=%.10f abs_diff=%.3e %s\n",
                     e.param_name.c_str(), e.analytic, e.finite_difference, e.abs_difference,
                     e.within_tolerance ? "OK" : "FAIL");
    }
    return result.gradient_check.all_within_tolerance ? 0 : 1;
}
