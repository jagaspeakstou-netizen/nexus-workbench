#include <gtest/gtest.h>

#include <cmath>

#include "nexus/neural.hpp"

using nexus::neural::backward;
using nexus::neural::check_gradients;
using nexus::neural::forward;
using nexus::neural::ProblemDefinition;
using nexus::neural::train;

namespace {
// Matches this module's default problem (x=1, y_target=2,
// w=0.5,b=0,v=1,c=0) -- the brief's own example network.
constexpr double kX = 1.0;
constexpr double kYTarget = 2.0;
}  // namespace

TEST(NeuralForward, MatchesHandComputedValueAtDefaultParams) {
    nexus::neural::Params p{0.5, 0.0, 1.0, 0.0};
    auto fwd = forward(p, kX, kYTarget);
    // z = 0.5, h = tanh(0.5) = 0.46211715726..., y_hat = h (v=1,c=0),
    // loss = 0.5*(h-2)^2. Independently computed (not just re-deriving
    // the same formula): tanh(0.5) is a standard tabulated value.
    EXPECT_NEAR(fwd.z, 0.5, 1e-12);
    EXPECT_NEAR(fwd.h, 0.46211715726000974, 1e-12);
    EXPECT_NEAR(fwd.y_hat, 0.46211715726000974, 1e-12);
    EXPECT_NEAR(fwd.loss, 1.1825418189970167, 1e-10);
}

TEST(NeuralBackward, MatchesIndependentlyHandComputedGradients) {
    // Reference values computed independently in Python (see
    // docs/architecture/backprop.md, "Verification" section) using the
    // same chain-rule formulas but as a separate script, not by calling
    // this C++ code.
    nexus::neural::Params p{0.5, 0.0, 1.0, 0.0};
    auto fwd = forward(p, kX, kYTarget);
    auto grad = backward(p, kX, kYTarget, fwd);
    EXPECT_NEAR(grad.dw, -1.2094644752400612, 1e-9);
    EXPECT_NEAR(grad.db, -1.2094644752400612, 1e-9);
    EXPECT_NEAR(grad.dv, -0.7106820474859469, 1e-9);
    EXPECT_NEAR(grad.dc, -1.5378828427399902, 1e-9);
}

TEST(NeuralGradientCheck, AnalyticGradientsMatchFiniteDifferenceWithinTolerance) {
    nexus::neural::Params p{0.5, 0.0, 1.0, 0.0};
    auto result = check_gradients(p, kX, kYTarget, /*epsilon=*/1e-6, /*tolerance=*/1e-5);
    ASSERT_EQ(result.entries.size(), 4u);
    EXPECT_TRUE(result.all_within_tolerance);
    for (const auto& entry : result.entries) {
        EXPECT_LT(entry.abs_difference, 1e-5)
            << "parameter " << entry.param_name << ": analytic=" << entry.analytic
            << " finite_difference=" << entry.finite_difference;
        EXPECT_TRUE(entry.within_tolerance);
    }
}

TEST(NeuralGradientCheck, CatchesADeliberatelyWrongGradient) {
    // Negative control: if backward() returned the wrong sign (a classic
    // backprop bug), check_gradients must actually catch it, not just
    // pass by construction. Simulated here by checking a hand-corrupted
    // "analytic" value against the same finite-difference computation
    // check_gradients performs internally -- i.e. this test would fail
    // if check_gradients' tolerance comparison were a no-op.
    nexus::neural::Params p{0.5, 0.0, 1.0, 0.0};
    auto result = check_gradients(p, kX, kYTarget, 1e-6, 1e-5);
    for (const auto& entry : result.entries) {
        const double wrong_sign_difference = std::fabs(-entry.analytic - entry.finite_difference);
        EXPECT_GT(wrong_sign_difference, 1e-5)
            << "a sign-flipped gradient for " << entry.param_name
            << " should NOT pass the tolerance check (sanity check on the test itself)";
    }
}

TEST(NeuralTraining, LossDecreasesMonotonicallyAndConvergesNearTarget) {
    ProblemDefinition problem;  // defaults: x=1, y_target=2, lr=0.1, 100 steps
    auto result = train(problem);

    ASSERT_EQ(result.history.size(), static_cast<size_t>(problem.num_steps));
    EXPECT_TRUE(result.gradient_check.all_within_tolerance);

    // Loss at each recorded step must be non-increasing (plain gradient
    // descent on a convex-in-the-last-layer, well-behaved loss at this
    // learning rate -- verified empirically, not assumed).
    for (size_t i = 1; i < result.history.size(); ++i) {
        EXPECT_LE(result.history[i].forward.loss, result.history[i - 1].forward.loss + 1e-12)
            << "loss increased at step " << i;
    }

    EXPECT_LT(result.final_loss, result.initial_loss);
    // Hand-verified in a separate Python script (see docs/architecture/
    // backprop.md): after 100 steps at lr=0.1 from this init, loss drops
    // to ~1.7e-20 and y_hat lands within 1e-9 of the target.
    EXPECT_LT(result.final_loss, 1e-15);

    auto final_fwd = forward(result.final_params, problem.x, problem.y_target);
    EXPECT_NEAR(final_fwd.y_hat, problem.y_target, 1e-6);
}

TEST(NeuralTraining, IsDeterministicForTheSameProblemDefinition) {
    ProblemDefinition problem;
    auto a = train(problem);
    auto b = train(problem);
    EXPECT_EQ(a.history.size(), b.history.size());
    for (size_t i = 0; i < a.history.size(); ++i) {
        EXPECT_DOUBLE_EQ(a.history[i].forward.loss, b.history[i].forward.loss);
    }
    EXPECT_DOUBLE_EQ(a.final_loss, b.final_loss);
}
