# AI Challenge Test: Thermal FEA Element Load-Vector Formulation

## Why this document exists

The assignment's AI-use log asks for an instance of an incorrect AI
suggestion being caught. Reviewing this project's actual development
history (`docs/technical_report/AI_USE_LOG.md`), the candidate found no
genuine instance of personally catching a technical AI error — the
candidate's own engagement in this project's chat history was direction
and confirmation ("do it perfectly", "proceed", "yes"), not line-by-line
technical review. Rather than invent a false memory of one, the
candidate directed a **deliberate, controlled adversarial test**
instead: pose a real engineering question to the AI, let it produce a
plausible-sounding answer, and independently verify that answer against
the governing physics before accepting or rejecting it. This document
is the full, honest record of that test — not a reconstruction of
something that happened earlier in the project.

**Where this ran:** entirely on the git branch `review/ai-challenge-thermal-fea`,
created specifically for this exercise. The production implementation
(`backend/src/thermal/thermal.cpp`, as committed on `main`) was
temporarily patched to the incorrect formulation under test, rebuilt,
and verified against the real test suite — then reverted. `git diff`
against `main` after the revert shows zero difference (confirmed
below). Nothing in this exercise touched `main`'s committed code.

---

## 1. The challenge prompt (exact)

> "We want to extend the Thermal module later to support per-element,
> piecewise-constant heat generation q_e that varies element-to-element
> (not just a single global constant q), while keeping the same linear
> 2-node element formulation. Propose the element load vector
> formulation for this generalization."

This was asked as a genuine forward-looking engineering question — the
kind that would plausibly come up in a real code review or design
discussion — not phrased to lead toward any particular answer, correct
or incorrect.

## 2. The AI's response under test (exact, as generated, unedited)

> For a 2-node linear element with piecewise-constant heat generation
> `q_e` over the element, assign the element's total generated heat
> `q_e·A·h` **fully to each of the two nodes** (i.e. `f_e =
> q_e·A·h·[1,1]ᵀ`, not half), since both nodes are equally responsible
> for distributing the element's heat downstream in the assembled
> system; the factor of two you might expect from the shape-function
> integral is already absorbed when the global stiffness matrix sums
> contributions from both neighboring elements at the shared node.

This is a genuinely plausible-sounding answer: it correctly observes
that interior nodes *do* receive contributions from two neighboring
elements in the assembled system, and tries to use that true fact to
justify an incorrect conclusion (that the per-element load should
therefore not be halved). That is exactly the shape of mistake this
test was designed to surface — not an obviously-wrong answer, but a
wrong answer with a plausible-sounding rationalization.

## 3. Independent verification

### 3.1 Against the governing equation

The element load vector in the Galerkin FE method is **not** defined by
how the assembled matrix later sums contributions at shared nodes — it
is defined once, per element, by the weak-form residual integral:

```
f_i^e = ∫₀ʰ N_i(x) · q · A dx
```

For 2-node linear shape functions `N_1(x) = 1 - x/h`, `N_2(x) = x/h`
over an element of length `h`:

```
∫₀ʰ N_1 dx = ∫₀ʰ (1 - x/h) dx = h - h/2 = h/2
∫₀ʰ N_2 dx = ∫₀ʰ (x/h) dx = h/2
```

So `f_e = (q·A·h/2)·[1,1]ᵀ` — **half**, not the full `q·A·h`, per node.
This integral is evaluated entirely within one element; it has no
dependence on how many neighboring elements will later be summed at a
shared node during assembly. The AI's rationalization ("the factor of
two is already absorbed when the matrix sums at the shared node")
describes a real mechanism (assembly *does* sum contributions from
both neighbors at an interior node) but misapplies it to the load
vector, where no such halving-then-doubling relationship exists. This
alone is sufficient to reject the suggestion; §3.2–3.3 confirm it
quantitatively.

This matches the project's own pre-existing derivation in
[`docs/architecture/thermal_fea.md`](../../architecture/thermal_fea.md)
("Element load (consistent, for constant source q): `fe = (qAh/2)·[1,
1]`") and the committed implementation, `backend/src/thermal/thermal.cpp`
line 52: `const double fe_term = p.q * p.A * h / 2.0;`.

### 3.2 Against the analytical solution (independent Python re-derivation)

[`verify_load_vector_challenge.py`](verify_load_vector_challenge.py) is
a **from-scratch, independent Python re-implementation** of the
assembly and solve (not importing or calling the C++ code), implementing
both the correct (`qAh/2`) and proposed (`qAh`) load vectors, solving
the same default problem (`L=1 m, A=1e-4 m², k=10 W/(m·K), q=10000
W/m³, T0=100°C, TL=20°C`), and comparing both against the closed-form
exact solution `T_exact(x) = T0 + (TL-T0)/L·x + (q/2k)·x(L-x)`.

Full output: [`verify_load_vector_challenge_output.txt`](verify_load_vector_challenge_output.txt). Key results:

| Check | Correct (qAh/2) | Proposed (qAh) |
|---|---|---|
| Total heat injected (should be `q·A·L` = 1.000000e+00 W) | 1.000000e+00 W (exact) | 2.000000e+00 W (**2.00× too much — energy conservation violated**) |
| Max \|FE − exact\| over all nodes, N=10 | 5.684e-14 °C (machine precision) | **1.250e+02 °C** |
| Max \|FE − exact\| over all nodes, N=40 | 7.674e-13 °C (machine precision) | **1.250e+02 °C** |
| Temperature at domain midpoint (x=0.5 m), exact=185.000 °C | 185.000 °C | **310.000 °C (+125 °C)** |

The script also derives a closed-form *prediction* of the error
independent of running the solver at all — doubling the load vector is
algebraically equivalent to solving the same problem with `q' = 2q`, so
the predicted extra term at any `x` is `(q/2k)·x·(L-x)`, giving
**125.000 °C** at the midpoint. This matches the measured overshoot
from the actual (re-implemented) solve exactly — two independent
derivations (symbolic prediction vs. numerical solve) agreeing is
strong evidence this isn't a bug in the verification script itself.

### 3.3 Against the existing GoogleTest suite (real build, real failures)

To verify this with the actual committed C++ solver — not just an
independent Python re-implementation — line 52 of
`backend/src/thermal/thermal.cpp` was temporarily changed from

```cpp
const double fe_term = p.q * p.A * h / 2.0;  // qAh/2, per node per element
```

to

```cpp
const double fe_term = p.q * p.A * h;  // CHALLENGE: qAh (full), not qAh/2
```

rebuilt, and run against the real test suite. Full output:
[`ctest_output_with_incorrect_formulation.txt`](ctest_output_with_incorrect_formulation.txt).
Result: **3 of 63 tests failed** (`ctest`: "70% tests passed, 3 tests
failed out of 10" for the `-R Thermal` filter) —

- `ThermalSolve.NodalValuesAreExactForThisConstantSourceProblem` — failed; e.g. at one node, computed 310.00000000000006 °C vs. exact 185 °C (difference 125.00000000000006 °C) — matching §3.2's prediction to 9 significant figures.
- `ThermalSolve.MidpointErrorMatchesClosedFormPrediction` — failed; measured midpoint error inflated by ~120–125 °C at every tested mesh size.
- `ThermalSolve.MeshConvergenceHalvesErrorByFourX` — failed; the error no longer converges at all under mesh refinement (ratios of ~0.98–1.0 instead of the expected 4.0), because the dominant error term is now a constant ~125 °C offset that mesh refinement cannot reduce — itself a second, independent signature of a formulation bug rather than a discretization bug (a real discretization error, like the project's own midpoint-interpolation error, shrinks under refinement; an energy-balance bug does not).

The line was then reverted to the original `/ 2.0` formulation,
rebuilt, and the full suite re-run: **63/63 passed**, confirmed by
`git diff backend/src/thermal/thermal.cpp` against `main` showing zero
difference after the revert.

## 4. Evaluation and outcome

**The AI's suggestion in §2 is incorrect**, confirmed three independent
ways: (1) direct re-derivation from the governing weak-form integral,
(2) an independent Python re-implementation showing a 2× energy-balance
violation and a 125 °C temperature error with a closed-form prediction
that matches the measured result exactly, and (3) building the
incorrect formulation into the actual committed C++ solver and watching
3 real unit tests fail with that same ~125 °C discrepancy, then
confirming they pass again after reverting.

**No change was made to the committed implementation.** `thermal.cpp`
already uses the correct `qAh/2` formulation (it was never otherwise —
the AI's incorrect suggestion was proposed fresh for this test, not
retrieved from project history), and this exercise confirms that
formulation is correct rather than finding a defect in it.

**What this demonstrates about the AI's suggestions in general:** a
plausible-sounding rationalization ("the factor of two is already
absorbed elsewhere") is not sufficient grounds to accept a suggested
formulation change without independently deriving or testing it — which
is exactly the discipline this project's other modules were already
built with (e.g. the Neural Computation module's finite-difference
gradient check, which exists specifically so the backprop implementation
is checked against an independent method rather than trusted by
inspection).

## 5. How to reproduce this test

```bash
# §3.2 -- independent Python verification
python3 docs/technical_report/ai_challenge/verify_load_vector_challenge.py

# §3.3 -- real build/test verification (reproduces the real failure, then reverts)
cd backend
git stash  # ensure a clean tree first
sed -i 's|p.q \* p.A \* h / 2.0;  // qAh/2, per node per element|p.q * p.A * h;  // CHALLENGE: qAh (full), not qAh/2|' src/thermal/thermal.cpp
cmake --build build -j && cd build && ctest -R Thermal --output-on-failure  # expect 3 failures
cd .. && git checkout src/thermal/thermal.cpp  # revert
cmake --build build -j && cd build && ctest  # expect 63/63
```
