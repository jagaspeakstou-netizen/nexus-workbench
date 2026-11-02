# NEXUS — Engineering Intelligence Workbench
### Technical Report
PASS Science Works R&D Internship 2026–2027 (Web & Software Development track)
Candidate: Jagadeeshvar Muralidharan
Repository: https://github.com/jagaspeakstou-netizen/nexus-workbench

---

## 1. Overview

NEXUS is a C++17/React engineering simulation workbench implementing six
independent engineering-discipline modules behind a single HTTP API and a
single web UI, plus a native Windows packaging path. Every number in this
report — test counts, CI run results, convergence rates, benchmark
timings — is drawn from a real, reproducible run recorded in this
repository (CSV files under `results/`, commit history, or a linked
GitHub Actions run), not estimated or asserted from memory. Where a
limitation was found during development, it is disclosed here and in the
relevant module's own architecture doc rather than edited out.

**Stack:** C++17 core (`backend/`, static lib `nexus_core` + a thin
`cpp-httplib`/`nlohmann::json` HTTP layer), React + TypeScript + Vite +
Tailwind frontend (`frontend/`), GoogleTest for all backend testing.

**Module status** (from `backend/src/core/status.cpp`, the single
code-driven source the UI's Overview page itself renders — not a
separately maintained claim):

| Module | Status | Tests |
|---|---|---|
| Geometry Lab | Implemented, tested | included in 63 |
| Thermal Analysis | Implemented, tested | included in 63 |
| Performance Lab | Implemented, tested | included in 63 |
| Hardware Integration (mock) | Implemented, tested | included in 63 |
| Neural Computation | Implemented, tested | included in 63 |
| Validation & Tests | Complete (Day 11 pass) | see §5 |
| Documentation | This report | — |

**Total backend test count: 63/63 passing**, verified on both this
project's Linux dev container (plain build, a strict-warnings build, and
under AddressSanitizer+UndefinedBehaviorSanitizer) and on a real
`windows-latest` GitHub Actions runner (see §5 and §6).

---

## 2. Module summaries

Full derivations and implementation notes live in each module's own
architecture doc, linked below; this section gives the essential result
each one proves.

### 2.1 Geometry Lab
*Full doc: [`docs/architecture/geometry_lab.md`](../architecture/geometry_lab.md)*

Hand-rolled, line-numbered ASCII STL and ANSYS nodal-CSV parsers with
strict grammar/schema validation (malformed input is rejected with a
specific, located error — never silently coerced or crashed on). Real
Three.js viewers (`MeshViewer`, `PointCloudViewer`) render the parsed
JSON directly, with orbit controls, fit-to-view, and click-to-inspect
raycasting on the point cloud.

### 2.2 Thermal Analysis
*Full doc: [`docs/architecture/thermal_fea.md`](../architecture/thermal_fea.md)*

1D steady-state FEA solver for `-d/dx(kA dT/dx) = qA` with Dirichlet
boundaries, solved via symmetric elimination + the Thomas algorithm on
the resulting tridiagonal system. The solver is nodally exact for this
constant-source problem; the brief's required convergence metric — FE
interpolation error at **element midpoints** — is checked against an
independently-derived closed form `error(h) = q·h²/(8k)`, not just
against the solver's own output. Measured midpoint error at
N=10/20/40/80: **1.25, 0.3125, 0.078125, 0.01953125 °C** — each exactly
4× smaller than the last, confirming the expected O(h²) rate.

![Thermal FE vs exact solution and midpoint error](../../results/plots/thermal_profile_and_midpoint_error_n40.png)
![Thermal mesh convergence, log-log, O(h²) reference](../../results/plots/thermal_mesh_convergence.png)

### 2.3 Performance Lab
*Full doc: [`docs/architecture/performance_lab.md`](../architecture/performance_lab.md)*

Runs a reproducible (fixed-seed) batch of independent thermal solves
both serially and across a bounded `std::thread` work-stealing pool,
with deterministic result ordering (each worker writes to its own
pre-assigned slot, never `push_back`) and an exact numerical-agreement
check between the two paths (**0.0 °C difference measured** — both paths
call the identical solver on identical inputs). Worker counts are capped
to `std::thread::hardware_concurrency()`, never exceeding the machine's
actual logical CPU count.

This project's dev container reports 2 logical CPUs; at 4000 cases ×
N=200 elements, the most recent recorded run measured **1.30× speedup /
65.1% efficiency at 2 workers**. An earlier under-scaled version of this
same benchmark (40 cases × N=40) measured *slower* parallel execution
(thread-launch overhead exceeded the tiny per-case work) — a genuine
benchmark-design mistake found and fixed during development, documented
in the module's own doc rather than removed from the record.

![Performance Lab speedup and efficiency](../../results/plots/performance_speedup_efficiency.png)

### 2.4 Neural Computation
*Full doc: [`docs/architecture/backprop.md`](../architecture/backprop.md)*

Hand-derived backpropagation (no autodiff library) for a single hidden
unit `x → (w,b) → z → tanh → h → (v,c) → ŷ`, `L = 0.5(ŷ-y)²`. Every
analytic gradient is checked against a central finite-difference
approximation (`ε = 1e-6`, tolerance `1e-5`) — and a **negative-control
test** (`NeuralGradientCheck.CatchesADeliberatelyWrongGradient`) proves
the check would actually fail on a wrong gradient, rather than being a
check that can never fail. 100-step plain gradient descent converges the
loss from 1.18 to effectively 0 (see plot).

![Neural training loss curve](../../results/plots/neural_training_loss.png)
![Neural analytic vs finite-difference gradient check](../../results/plots/neural_gradient_check.png)

### 2.5 Hardware Integration
*Full doc: [`docs/architecture/hardware_mocks.md`](../architecture/hardware_mocks.md)*

**No physical hardware is attached to this project.** This module is a
deterministic (seeded, not randomized) software simulator implementing
the discover → connect (scripted retry) → telemetry (scripted
mid-session dropout + automatic reconnect) → pinout adapter pattern for
Microcontroller/FPGA/JTAG device classes. This is disclosed at every
layer: the API's own `mock_disclaimer` field, the frontend banner, the
header comment, and the architecture doc's title — never presented as,
or mistakeable for, real hardware telemetry.

![Hardware telemetry with simulated dropout and recovery](../../results/plots/hardware_telemetry_dropout_recovery.png)

A genuine Windows-only compile failure was found and fixed in this
module during development (§6.2) — `<combaseapi.h>`'s `#define interface
struct` COM macro collided with a struct field literally named
`interface`, compiling fine on Linux/GCC but breaking MSVC with a wall of
cascading syntax errors. Fixed by renaming the C++ identifier (the JSON
wire field stayed `"interface"` for frontend compatibility). Left in the
record rather than silently fixed, per this project's disclosure policy.

### 2.6 Windows packaging
*Full doc: [`docs/packaging/windows.md`](../packaging/windows.md)*

A native WebView2 shell (`Launch.exe`, `windows/launcher/`) that spawns
`nexus_server.exe` and displays the built frontend — no terminal, admin
rights, internet access, or install step required on a clean Windows 11
machine. Statically-linked MSVC CRT (no VC++ Redistributable dependency).
Built and verified end-to-end on a real `windows-latest` GitHub Actions
runner, including a 6-second "still running, not crashed" liveness check
and a full-screen screenshot artifact (see §6.1 for what was and wasn't
independently verifiable about that artifact).

---

## 3. Architecture

```
┌─────────────────────────┐        HTTP/JSON        ┌───────────────────────────┐
│  frontend/ (React+TS)    │  ───────────────────▶   │  backend/src/api/main.cpp │
│  src/services/api.ts     │  ◀───────────────────   │  (nexus_server)           │
│  — one page per module   │                          └─────────────┬─────────────┘
└─────────────────────────┘                                         │
                                                         ┌───────────▼─────────────┐
                                                         │   nexus_core (static)    │
                                                         │  geometry / thermal /    │
                                                         │  performance / neural /  │
                                                         │  hardware / status       │
                                                         └───────────────────────────┘
```

All computation lives in `nexus_core`; `main.cpp` only wires HTTP routes
to it, so every module is independently unit-testable without the HTTP
layer (the architecture every `test_<module>.cpp` file exercises
directly). On Windows, `windows/launcher/` adds a `nexus_launcher`
(`Launch.exe`) target that spawns `nexus_server.exe` as a child process
and hosts the built frontend in a WebView2 control — see §2.6.

---

## 4. Build and run

See the top-level [`README.md`](../../README.md) for the full
prerequisites and commands. Summary:

```bash
# Backend
cd backend && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j && ./build/nexus_server 8787

# Backend tests
cd backend/build && ctest --output-on-failure

# Frontend
cd frontend && npm install && npm run dev
```

Windows: run `Launch.exe` from a built release (§2.6), or build from
source in VS Code/Antigravity per
[`docs/packaging/windows.md`](../packaging/windows.md#editor-based-source-build-vs-code-or-antigravity).

---

## 5. Validation (Day 11 pass)

Full detail: [`docs/architecture/validation.md`](../architecture/validation.md).
Summary of what was actually run, with results:

| Check | Result |
|---|---|
| `-Wall -Wextra -Wpedantic -Wshadow` | 1 real bug found and fixed (a shadowed loop variable in `hardware_mocks.cpp`); 0 warnings after |
| AddressSanitizer + UndefinedBehaviorSanitizer | 63/63 tests clean, no leaks |
| ThreadSanitizer | 19 concurrency-relevant tests + the real 4000-case multicore benchmark CLI run, clean, bit-exact serial/parallel agreement |
| Edge-case audit | 2 genuine Thermal gaps filled (minimum-valid N=1 mesh; zero-heat-source exact-linear regime); 1 genuine Performance bug found and fixed (`run_batch_parallel(num_workers<=0)` previously returned silently-unpopulated zero results instead of erroring — now throws `std::invalid_argument`) |

Every finding above is backed by a named test or a quoted compiler/tool
log in `validation.md` — this section is a summary of that document, not
a separate claim.

---

## 6. Cross-platform CI evidence (GitHub Actions, `windows-latest`)

This project's only genuinely independent verification of Windows
behavior — this dev environment itself is Linux — comes from
`.github/workflows/windows-build.yml` running on GitHub-hosted Windows
runners. Three runs are relevant to this report:

### 6.1 Initial packaging verification
- **Run 1** ([41338201277](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41338201277)) — failed: hardcoded CMake generator `"Visual Studio 17 2022"` didn't match that runner's actually-installed VS 18. Fixed by letting CMake auto-detect the generator.
- **Run 2** ([41338521566](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41338521566)) — passed: MSVC 19.51.36260.0, 43/43 tests, `Launch.exe` confirmed still running 6 seconds after launch, statically-linked WebView2 loader confirmed. **Disclosed gap:** the run's screenshot artifact could not be downloaded in this environment (its Azure Blob Storage host is outside this session's network allowlist) — its *existence* is verified via the workflow's own pass/fail gate, but its visual contents were never personally inspected here; the candidate should pull it from the Actions UI before relying on it in an interview.

### 6.2 Hardware Integration module
- A run against commit `dec8171` failed to compile: the `interface` field / COM macro collision described in §2.5.
- The fix (commit `11138f1`) then passed: **60/60 tests on real Windows**.

### 6.3 Day 11 validation pass
- Run [41373581581](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41373581581) against commit `01a865e` — passed: **63/63 tests on real Windows**, including the new `BatchRun.RejectsInvalidWorkerCount` regression test. Confirmed by reading the run's own log directly (`gh run view --log`), not by the green checkmark alone.

---

## 7. Known limitations (disclosed, not hidden)

- **Hardware Integration is fully simulated** — no physical device was ever attached (§2.5).
- **Performance Lab's absolute timings are container-specific** — this dev environment reports only 2 logical CPUs on what is very likely a shared, virtualized host; two otherwise-identical runs produced 1.30×/65% and 1.80×/90% speedup at different moments. The numerical-agreement result (0.0 °C) is the load-bearing claim, not the absolute speedup figure.
- **The Windows CI screenshot artifact's visual contents were never inspected from this environment** (§6.1) — only its existence and the pass/fail gate around it.
- **End-to-end Playwright verification of every frontend page against a live backend has not yet been run as part of this pass** — unit/integration test coverage at the C++ layer is complete (63/63), but a full UI click-through pass is a remaining item, tracked separately from this report.
- **Rubric self-scoring is not included in this report** — it requires the assignment's own grading rubric, which this environment does not currently have a copy of.

---

## 8. Reproducing this report's evidence

```bash
# Regenerate every CSV cited above
cd backend && cmake --build build -j
./build/nexus_thermal_export ../results/csv
./build/nexus_neural_export ../results/csv
./build/nexus_hardware_export ../results/csv
./build/nexus_performance_export ../results/benchmarks

# Regenerate the plots in this report from those CSVs
python3 scripts/generate_plots.py   # from the repo root; needs matplotlib

# Full test suite
cd backend/build && ctest --output-on-failure
```

See [`docs/architecture/validation.md`](../architecture/validation.md) for
the sanitizer/warnings build commands.
