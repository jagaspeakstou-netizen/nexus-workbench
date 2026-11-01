// Module F -- Neural Computation (hand-implemented backpropagation).
//
// Deliberately the smallest network for which forward/backward/gradient
// check/training all mean something: one input, one tanh hidden unit,
// one linear output.
//
//     x --(w,b)--> z = w*x + b --tanh--> h --(v,c)--> y_hat = v*h + c
//
// Loss is mean-squared error against a fixed target: L = 0.5*(y_hat - y)^2
// (the 0.5 is the usual convention that cancels the derivative's factor
// of 2, kept for a cleaner hand derivation -- see docs/architecture/
// backprop.md for the full chain-rule derivation this header implements).
//
// backward() is NOT autodiff and does NOT call any differentiation
// library -- every partial derivative is written out by hand, matching
// the derivation document line for line. check_gradients() independently
// verifies those hand-derived partials against central-difference
// numerical gradients, which is the actual correctness check for this
// module (comparing the analytic implementation to itself would prove
// nothing).

#pragma once

#include <string>
#include <vector>

namespace nexus::neural {

struct Params {
    double w = 0.5;
    double b = 0.0;
    double v = 1.0;
    double c = 0.0;
};

struct Gradients {
    double dw = 0.0;
    double db = 0.0;
    double dv = 0.0;
    double dc = 0.0;
};

// Everything forward() computes, kept around rather than discarded,
// because backward() needs h and y_hat and the frontend/CSV export want
// to show them too -- recomputing silently inside backward() would hide
// the fact that backprop reuses the forward pass's intermediates.
struct ForwardResult {
    double z = 0.0;      // w*x + b
    double h = 0.0;      // tanh(z)
    double y_hat = 0.0;  // v*h + c
    double loss = 0.0;   // 0.5*(y_hat - y_target)^2
};

struct ProblemDefinition {
    double x = 1.0;
    double y_target = 2.0;
    Params init_params{};
    double learning_rate = 0.1;
    int num_steps = 100;
};

ForwardResult forward(const Params& p, double x, double y_target);

// Hand-derived chain-rule gradients of the loss at `fwd` (the forward
// pass already computed at `p`) with respect to w, b, v, c.
Gradients backward(const Params& p, double x, double y_target, const ForwardResult& fwd);

struct GradientCheckEntry {
    std::string param_name;
    double analytic = 0.0;
    double finite_difference = 0.0;
    double abs_difference = 0.0;
    bool within_tolerance = false;
};

struct GradientCheckResult {
    double epsilon = 0.0;
    double tolerance = 0.0;
    std::vector<GradientCheckEntry> entries;
    bool all_within_tolerance = false;
};

// Central-difference check of backward()'s analytic gradients against
// numerical ones: for each parameter p_i,
//   grad_fd = (L(p_i + epsilon) - L(p_i - epsilon)) / (2*epsilon)
// compared to backward()'s dL/dp_i, each required to agree within
// `tolerance`. Evaluated at the problem's initial parameters, before any
// training step has moved them.
GradientCheckResult check_gradients(
    const Params& p, double x, double y_target,
    double epsilon = 1e-6, double tolerance = 1e-5);

struct TrainingStepRecord {
    int step = 0;
    Params params_before;  // parameters used for this step's forward/backward
    ForwardResult forward;
    Gradients grad;
};

struct TrainingResult {
    ProblemDefinition problem;
    GradientCheckResult gradient_check;  // run once, at init_params
    std::vector<TrainingStepRecord> history;
    Params final_params;
    double initial_loss = 0.0;
    double final_loss = 0.0;
};

// Plain (full-batch, single-example) gradient descent for
// problem.num_steps steps: params -= learning_rate * gradient, each step.
TrainingResult train(const ProblemDefinition& problem = ProblemDefinition{});

void export_training_csv(const TrainingResult& result, const std::string& path);
void export_gradient_check_csv(const GradientCheckResult& result, const std::string& path);

}  // namespace nexus::neural
