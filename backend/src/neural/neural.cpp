#include "nexus/neural.hpp"

#include <cmath>
#include <fstream>
#include <stdexcept>

namespace nexus::neural {

ForwardResult forward(const Params& p, double x, double y_target) {
    ForwardResult r;
    r.z = p.w * x + p.b;
    r.h = std::tanh(r.z);
    r.y_hat = p.v * r.h + p.c;
    const double diff = r.y_hat - y_target;
    r.loss = 0.5 * diff * diff;
    return r;
}

Gradients backward(const Params& p, double x, double y_target, const ForwardResult& fwd) {
    // Hand-derived chain rule (full derivation in
    // docs/architecture/backprop.md -- this is a direct transcription of
    // it, not a reduced/simplified form):
    //
    //   dL/dy_hat = (y_hat - y_target)              [L = 0.5*(y_hat-y)^2]
    //   dL/dv     = dL/dy_hat * h                     [y_hat = v*h + c]
    //   dL/dc     = dL/dy_hat * 1
    //   dL/dh     = dL/dy_hat * v
    //   dh/dz     = 1 - tanh(z)^2 = 1 - h^2            [h = tanh(z)]
    //   dL/dz     = dL/dh * dh/dz
    //   dL/dw     = dL/dz * x                          [z = w*x + b]
    //   dL/db     = dL/dz * 1
    const double dL_dyhat = fwd.y_hat - y_target;
    const double dL_dv = dL_dyhat * fwd.h;
    const double dL_dc = dL_dyhat;
    const double dL_dh = dL_dyhat * p.v;
    const double dh_dz = 1.0 - fwd.h * fwd.h;
    const double dL_dz = dL_dh * dh_dz;
    const double dL_dw = dL_dz * x;
    const double dL_db = dL_dz;

    Gradients g;
    g.dw = dL_dw;
    g.db = dL_db;
    g.dv = dL_dv;
    g.dc = dL_dc;
    return g;
}

GradientCheckResult check_gradients(const Params& p, double x, double y_target,
                                     double epsilon, double tolerance) {
    GradientCheckResult result;
    result.epsilon = epsilon;
    result.tolerance = tolerance;

    const ForwardResult fwd0 = forward(p, x, y_target);
    const Gradients analytic = backward(p, x, y_target, fwd0);

    auto check_one = [&](const char* name, double analytic_value,
                          double Params::*member) -> GradientCheckEntry {
        Params plus = p;
        Params minus = p;
        plus.*member += epsilon;
        minus.*member -= epsilon;
        const double loss_plus = forward(plus, x, y_target).loss;
        const double loss_minus = forward(minus, x, y_target).loss;
        const double fd = (loss_plus - loss_minus) / (2.0 * epsilon);

        GradientCheckEntry entry;
        entry.param_name = name;
        entry.analytic = analytic_value;
        entry.finite_difference = fd;
        entry.abs_difference = std::fabs(analytic_value - fd);
        entry.within_tolerance = entry.abs_difference < tolerance;
        return entry;
    };

    result.entries.push_back(check_one("w", analytic.dw, &Params::w));
    result.entries.push_back(check_one("b", analytic.db, &Params::b));
    result.entries.push_back(check_one("v", analytic.dv, &Params::v));
    result.entries.push_back(check_one("c", analytic.dc, &Params::c));

    result.all_within_tolerance = true;
    for (const auto& e : result.entries) {
        if (!e.within_tolerance) {
            result.all_within_tolerance = false;
        }
    }
    return result;
}

TrainingResult train(const ProblemDefinition& problem) {
    TrainingResult result;
    result.problem = problem;
    result.gradient_check = check_gradients(problem.init_params, problem.x, problem.y_target);

    Params current = problem.init_params;
    result.initial_loss = forward(current, problem.x, problem.y_target).loss;

    for (int step = 0; step < problem.num_steps; ++step) {
        TrainingStepRecord rec;
        rec.step = step;
        rec.params_before = current;
        rec.forward = forward(current, problem.x, problem.y_target);
        rec.grad = backward(current, problem.x, problem.y_target, rec.forward);

        current.w -= problem.learning_rate * rec.grad.dw;
        current.b -= problem.learning_rate * rec.grad.db;
        current.v -= problem.learning_rate * rec.grad.dv;
        current.c -= problem.learning_rate * rec.grad.dc;

        result.history.push_back(rec);
    }

    result.final_params = current;
    result.final_loss = forward(current, problem.x, problem.y_target).loss;
    return result;
}

void export_training_csv(const TrainingResult& result, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("neural::export_training_csv: could not open " + path);
    }
    out << "step,w,b,v,c,z,h,y_hat,loss,dw,db,dv,dc\n";
    for (const auto& rec : result.history) {
        out << rec.step << ','
            << rec.params_before.w << ',' << rec.params_before.b << ','
            << rec.params_before.v << ',' << rec.params_before.c << ','
            << rec.forward.z << ',' << rec.forward.h << ',' << rec.forward.y_hat << ','
            << rec.forward.loss << ','
            << rec.grad.dw << ',' << rec.grad.db << ',' << rec.grad.dv << ',' << rec.grad.dc
            << '\n';
    }
}

void export_gradient_check_csv(const GradientCheckResult& result, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("neural::export_gradient_check_csv: could not open " + path);
    }
    out << "param,analytic,finite_difference,abs_difference,within_tolerance\n";
    for (const auto& e : result.entries) {
        out << e.param_name << ',' << e.analytic << ',' << e.finite_difference << ','
            << e.abs_difference << ',' << (e.within_tolerance ? "true" : "false") << '\n';
    }
}

}  // namespace nexus::neural
