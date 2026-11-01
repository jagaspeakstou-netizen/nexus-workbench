# Neural Computation module — hand-derived backpropagation

Implements `backend/include/nexus/neural.hpp` / `backend/src/neural/neural.cpp`.

## The network

The smallest network for which forward, backward, a finite-difference
gradient check, and a training loop all mean something distinct: one
input, one `tanh` hidden unit, one linear output.

```
x --(w, b)--> z = w·x + b --tanh--> h = tanh(z) --(v, c)--> ŷ = v·h + c
```

against a fixed target `y`, with mean-squared loss (the `1/2` is the
standard convention that cancels the derivative's factor of 2, used here
purely to make the derivative line below read as `(ŷ − y)` rather than
`2(ŷ − y)`):

```
L = (1/2)(ŷ − y)²
```

Default problem (the brief's own example network, used as this module's
default `ProblemDefinition`): `x = 1`, `y = 2`, initial `w = 0.5, b = 0,
v = 1, c = 0`, learning rate `0.1`, `100` training steps.

## Forward pass

| Symbol | Definition | Value at (x=1, w=0.5, b=0, v=1, c=0) |
|---|---|---|
| `z`  | `w·x + b`           | `0.5·1 + 0 = 0.5` |
| `h`  | `tanh(z)`           | `tanh(0.5) = 0.46211715726…` |
| `ŷ`  | `v·h + c`           | `1·0.46211715726 + 0 = 0.46211715726` |
| `L`  | `(1/2)(ŷ − y)²`     | `(1/2)(0.46211715726 − 2)² = 1.18254181900…` |

## Backward pass — the hand derivation

Four parameters, four partial derivatives of `L`, built by the chain rule
working backward through the same graph the forward pass built forward
through. Each line only needs the *previous* line plus one local
derivative — that locality is the entire point of backprop, and is why
`backward()` in `neural.cpp` is a straight-line sequence of multiplications
with no loops or recursion.

**Step 1 — loss w.r.t. the output.**

```
L = (1/2)(ŷ − y)²
∂L/∂ŷ = (ŷ − y)          [power rule: d/dŷ (1/2)u² = u·(du/dŷ); u = ŷ−y, du/dŷ = 1]
```

**Step 2 — output layer (`ŷ = v·h + c`) w.r.t. its own parameters.**

```
∂ŷ/∂v = h        ∂ŷ/∂c = 1
```

Chain rule combines these with Step 1:

```
∂L/∂v = ∂L/∂ŷ · ∂ŷ/∂v = (ŷ − y)·h
∂L/∂c = ∂L/∂ŷ · ∂ŷ/∂c = (ŷ − y)·1 = (ŷ − y)
```

**Step 3 — propagate the gradient back through the output layer, to `h`.**

```
∂ŷ/∂h = v
∂L/∂h = ∂L/∂ŷ · ∂ŷ/∂h = (ŷ − y)·v
```

**Step 4 — through the `tanh` nonlinearity.** The derivative of `tanh` has
the closed form `d/dz tanh(z) = 1 − tanh(z)² = 1 − h²` (standard identity:
`tanh'(z) = sech²(z) = 1 − tanh²(z)`, and `h` already *is* `tanh(z)`, so no
second `tanh` evaluation is needed — `backward()` reuses `fwd.h` rather
than recomputing it, which is both cheaper and the reason `backward()`
takes the already-computed `ForwardResult` as an argument instead of
recomputing the forward pass itself).

```
∂h/∂z = 1 − h²
∂L/∂z = ∂L/∂h · ∂h/∂z = (ŷ − y)·v·(1 − h²)
```

**Step 5 — hidden layer (`z = w·x + b`) w.r.t. its own parameters.**

```
∂z/∂w = x        ∂z/∂b = 1
∂L/∂w = ∂L/∂z · ∂z/∂w = (ŷ − y)·v·(1 − h²)·x
∂L/∂b = ∂L/∂z · ∂z/∂b = (ŷ − y)·v·(1 − h²)
```

This is exactly the five-line sequence in `backward()`:

```cpp
const double dL_dyhat = fwd.y_hat - y_target;
const double dL_dv = dL_dyhat * fwd.h;
const double dL_dc = dL_dyhat;
const double dL_dh = dL_dyhat * p.v;
const double dh_dz = 1.0 - fwd.h * fwd.h;
const double dL_dz = dL_dh * dh_dz;
const double dL_dw = dL_dz * x;
const double dL_db = dL_dz;
```

## Numeric check, at the default initial parameters

```
ŷ = 0.46211715726,  y = 2  ⟹  (ŷ − y) = −1.53788284274
∂L/∂v = −1.53788284274 × 0.46211715726 = −0.71068204749
∂L/∂c = −1.53788284274
∂L/∂h = −1.53788284274 × 1 (v=1)        = −1.53788284274
1 − h² = 1 − 0.46211715726² = 0.78644773296
∂L/∂z = −1.53788284274 × 0.78644773296  = −1.20946447524
∂L/∂w = −1.20946447524 × 1 (x=1)        = −1.20946447524
∂L/∂b = −1.20946447524
```

These match `neural.cpp`'s `backward()` output exactly (see
`test_neural.cpp`'s `NeuralBackward.MatchesIndependentlyHandComputedGradients`,
and the independent Python re-derivation used to produce the table above,
which calls none of this project's C++ code).

## Verification: finite-difference gradient check

Comparing a hand-derived analytic gradient to itself proves nothing —
`check_gradients()` in `neural.cpp` instead compares each analytic partial
to an independent **central finite difference**:

```
∂L/∂p ≈ [L(p + ε) − L(p − ε)] / (2ε),   ε = 1e-6
```

evaluated by literally perturbing one parameter at a time and re-running
the *forward* pass only (`check_gradients()` never calls `backward()`
inside the perturbation loop — if it did, a bug shared between `forward()`
and `backward()` could cancel out and the check would pass despite being
wrong). Required to agree with the analytic value to within `1e-5`:

| Parameter | Analytic | Finite difference | &#124;difference&#124; |
|---|---|---|---|
| w | −1.2094644752 | −1.2094644753 | 1.09×10⁻¹⁰ |
| b | −1.2094644752 | −1.2094644753 | 1.09×10⁻¹⁰ |
| v | −0.7106820475 | −0.7106820474 | 5.25×10⁻¹¹ |
| c | −1.5378828427 | −1.5378828426 | 1.12×10⁻¹⁰ |

All four agree to roughly 10 significant figures — the residual is
exactly what you'd expect from central-difference truncation error at
`ε = 1e-6` (error scales as `O(ε²)`, i.e. ~1e-12 relative, consistent with
the ~1e-10 absolute differences above given these gradients' magnitude),
not a sign of anything wrong. `test_neural.cpp`'s
`NeuralGradientCheck.CatchesADeliberatelyWrongGradient` test exists
specifically to confirm the check isn't a no-op: it verifies that a
sign-flipped gradient would actually fail this same tolerance comparison.

## Training: 100 steps of plain gradient descent

```
w, b, v, c ← (w, b, v, c) − lr · (∂L/∂w, ∂L/∂b, ∂L/∂v, ∂L/∂c)
```

recomputed fresh at every step (`train()` in `neural.cpp`). Loss decreases
monotonically from `1.18254` to `1.70×10⁻²⁰`, and `ŷ` converges to the
target `y = 2` to within `1.8×10⁻¹⁰` by step 100 — see
`results/csv/neural_training.csv` for the full per-step trace (all 13
columns: step, the four parameters, the three forward intermediates,
loss, and the four gradients) and `results/csv/neural_gradient_check.csv`
for the table above, both generated by `nexus_neural_export` and
committed as raw, unedited evidence.

## Reproducing

```
cd backend/build
./nexus_neural_export ../../results/csv   # regenerates the CSVs above
cd .. && ctest --output-on-failure -R Neural
```
