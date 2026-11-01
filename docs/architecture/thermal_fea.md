# Thermal FEA module — formulation, assumptions, verification

Implements `backend/include/nexus/thermal.hpp` / `backend/src/thermal/thermal.cpp`.

## Problem

Steady-state 1D heat conduction in a rod with uniform volumetric generation:

```
-d/dx(kA dT/dx) = qA,   0 <= x <= L
T(0) = T0,  T(L) = TL
```

with `L=1 m, A=1e-4 m², k=10 W/(m·K), q=10000 W/m³, T0=100°C, TL=20°C`, all
constant (the assignment's fixed values).

**Assumptions:** constant cross-section and material properties along the
rod; insulated lateral surface (all generated heat exits through the two
ends); 1D (no radial temperature gradient); steady state.

## Analytical solution

Integrating twice and applying both Dirichlet BCs:

```
T_exact(x) = T0 + (TL - T0)/L · x + [q/(2k)] · x(L - x)
```

which for the given constants is `T_exact(x) = 100 - 80x + 500x(1-x)`.

## Finite element formulation

Two-node linear elements, uniform mesh, element length `h = L/N`.

- **Element stiffness** (from the weak form, linear shape functions):
  `Ke = (kA/h) · [[1, -1], [-1, 1]]`
- **Element load** (consistent, for constant source `q`):
  `fe = (qAh/2) · [1, 1]` — i.e. half the element's total generated heat
  goes to each node (exact integral of `q·A·N_i` over the element for
  linear `N_i`).
- **Assembly:** standard scatter-add into a tridiagonal global system (each
  node only couples to its immediate neighbours for 2-node 1D elements).
- **Boundary conditions:** enforced by *symmetric elimination*, not a
  penalty method — for each fixed dof, its row/column contribution is
  moved to the load vector of the neighbouring equations, then the row is
  replaced with an identity equation. This keeps the solved system
  interpretable (no large penalty number to pick or justify).
- **Solve:** the assembled system is exactly tridiagonal, so it is solved
  directly with the Thomas algorithm (O(N), no general linear-algebra
  library needed — using a dense/iterative solver here would be solving
  the wrong-shaped problem).

## Midpoint error, not just nodal error

The brief specifically requires the convergence error to be measured via
**FE interpolation at element midpoints**, not a nodal comparison. The
reason becomes visible from the numbers themselves: for *this* problem
(piecewise-linear elements, globally constant source `q`), the assembled
Galerkin system is nodally exact — `backend/tests/test_thermal.cpp`
(`NodalValuesAreExactForThisConstantSourceProblem`) checks this directly
and the generated `results/csv/thermal_n*.csv` files show nodal error at
floating-point noise level (~1e-14 °C). A nodal-only convergence check
would therefore show ~zero error at every mesh size and hide the real
discretization error entirely — which only shows up between nodes, where
the FE field is linear but the true solution is quadratic.

Because `T_exact` is quadratic with constant curvature
(`a = -q/(2k)` in `T = ax² + bx + c`), the midpoint interpolation error has
a closed form:

```
error(h) = |a| · h² / 4 = q·h² / (8k)
```

`test_thermal.cpp` (`MidpointErrorMatchesClosedFormPrediction`) checks the
solver's numerical output against this independently-derived formula to
1e-9, rather than only checking the solver against itself. Measured values
for N=10/20/40/80 (`results/csv/thermal_convergence.csv`): **1.25, 0.3125,
0.078125, 0.01953125 °C** — each exactly 4× smaller than the last, matching
the predicted O(h²) convergence rate for linear elements.

## Reproducing

```
cd backend && cmake --build build -j
./build/nexus_thermal_export ../results/csv   # regenerates the CSVs above
cd build && ctest --output-on-failure -R Thermal
```
