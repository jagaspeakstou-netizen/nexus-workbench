# Day 11 — Validation pass

This document records the hardening/validation pass run against the
backend after Modules E (Hardware Integration) and F (Neural Computation)
landed. It is not a new feature module; it's an audit of what was already
built, with every finding disclosed whether or not it turned into a code
change.

All commands below were actually run in this environment (Linux
dev container) during this pass; see each section for the real output.

## 1. Compiler warnings (`-Wall -Wextra -Wpedantic -Wshadow`)

```
cd backend && rm -rf build_warn && mkdir build_warn && cd build_warn
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Wshadow" ..
cmake --build . -j
```

**Found one real warning**, not a style nit:

```
backend/src/hardware/hardware_mocks.cpp: In function
'nexus::hardware::TelemetrySession nexus::hardware::read_telemetry(const ConnectResult&, int)':
backend/src/hardware/hardware_mocks.cpp:129:21: warning: declaration of 't' shadows a previous
local [-Wshadow]
  129 |     for (DeviceType t : {DeviceType::Microcontroller, DeviceType::Fpga, DeviceType::Jtag}) {
      |                     ^
backend/src/hardware/hardware_mocks.cpp:121:12: note: shadowed declaration is here
  121 |     double t = 0.0;
      |            ^
```

`read_telemetry()` uses `double t` as the simulated session clock, and a
nested loop (searching `discover_devices()` for the active device, by id)
reused the name `t` for its loop variable. Both compiled and ran
correctly under GCC's normal warning level — this is purely a
readability/maintenance hazard (a future edit to the loop could easily
start reading/writing the wrong `t`), which is exactly what `-Wshadow`
exists to catch. Fixed by renaming the loop variable to `candidate_type`.

Rebuilt clean (0 warnings) after the fix: see `backend/src/hardware/hardware_mocks.cpp`.

## 2. AddressSanitizer + UndefinedBehaviorSanitizer

```
cd backend && rm -rf build_asan && mkdir build_asan && cd build_asan
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" ..
cmake --build . -j
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ctest --output-on-failure
```

Result: **63/63 tests pass clean** — no memory errors, no leaks, no
undefined behavior, across every module (Geometry, Thermal, Performance,
Neural, Hardware, Status).

## 3. ThreadSanitizer (Performance Lab concurrency code)

```
cd backend && rm -rf build_tsan && mkdir build_tsan && cd build_tsan
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..
cmake --build . --target nexus_tests nexus_performance_export -j
ctest -R "BatchRun|Benchmark|Hardware" --output-on-failure
./nexus_performance_export   # the real 4000-case / 200-element multicore benchmark CLI
```

Result: clean. No data races reported across the 19 concurrency-relevant
unit tests, and the real full-scale benchmark run (the same workload used
for `results/csv/performance_*`) also ran clean under TSan, with
bit-exact (`max_abs_agreement_error_C = 0.0`) serial/parallel agreement —
the same `run_batch_serial`/`run_batch_parallel` worker-pool code that
`-fsanitize=thread` is specifically meant to stress.

## 4. Edge-case audit

Every existing `TEST()` name across all 6 test files was listed and read
to check for genuine coverage gaps (not padding — each addition below is
justified by a specific scenario the existing suite didn't already
cover).

### Found and fixed: Thermal module

- **`ThermalSolve.HandlesMinimumValidMeshOfOneElement`** — the smallest
  *valid* mesh (`num_elements = 1`, i.e. exactly 2 nodes, both on the
  Dirichlet boundary) was never exercised. `RejectsInvalidMesh` only
  covers `num_elements < 1`; it says nothing about whether the smallest
  legal value actually works. A Thomas-algorithm tridiagonal solver that
  assumes at least one interior (non-boundary) unknown could misbehave
  here. Verified it doesn't: both nodal values land exactly on the
  boundary conditions, and the single midpoint's error matches the same
  closed-form prediction used for every other mesh size — no
  special-casing needed for the degenerate case.
- **`ThermalSolve.ZeroHeatSourceProducesExactLinearProfileEverywhere`** —
  every other Thermal test uses the default `q = 10000` problem. With
  `q = 0`, the exact solution degenerates to a straight line, which lies
  *exactly* inside this solver's piecewise-linear element space — so,
  unlike the `q != 0` case (where only nodal values are exact and
  midpoints carry real interpolation error), the FE solution should be
  exact everywhere, nodes and midpoints alike, at any mesh size. Verified
  across `n ∈ {1, 3, 7, 20}`.

### Found and fixed: Performance module

- **`run_batch_parallel(cases, mesh_elements, num_workers)` with
  `num_workers <= 0`** — found by reading the implementation directly
  (`src/performance/performance.cpp`), not from a failing test. With
  `num_workers = 0`, the worker-thread loop (`for (int w = 0; w < num_workers; ++w)`)
  spawns zero threads, so the pre-sized `case_results` vector is never
  written to by anyone and comes back silently populated with
  default-constructed (all-zero) `CaseResult` entries — not an error,
  just quietly wrong-looking output that could be mistaken for a real
  (if strange) zero-temperature result. `run_full_benchmark()` itself
  never triggers this (it clamps every configured worker count to
  `>= 1`), so this was a latent footgun in the public function's
  contract rather than a bug reachable through the benchmark CLI/API —
  but it's exactly the kind of gap this validation pass exists to catch
  before a test, demo, or future caller hits it by accident.

  Fixed by adding an explicit guard — `throw std::invalid_argument` for
  `num_workers < 1` — matching the Thermal module's own established
  validation style (`solve()` throws for `num_elements < 1`), documented
  in `performance.hpp`, and covered by a new test,
  `BatchRun.RejectsInvalidWorkerCount`, in `test_performance.cpp`.

### Checked, no gap found

- **Hardware module**: re-read all 11 `Hardware*` tests
  (`test_hardware_mocks.cpp`) against `hardware_mocks.cpp`'s actual
  branches (scripted retry parity rule, dropout-only-at-`count >= 4`,
  reconnect success/failure paths, pinout tables for all 3 device
  classes). Every branch the implementation can take is already
  exercised by an existing test; no new gap found here during this pass.
- **Neural module**: re-read all 6 `Neural*` tests against
  `neural.cpp`'s forward/backward/gradient-check/train functions,
  including the negative-control test
  (`NeuralGradientCheck.CatchesADeliberatelyWrongGradient`) that proves
  the gradient check isn't a no-op. No new gap found.
- **Geometry module**: existing tests already cover malformed/empty
  input files for both the CSV and STL parsers; no additional gap
  identified in this pass.

## 5. Test count

63 tests total after this pass (60 before Day 11 → +3: the two new
Thermal edge cases, and `BatchRun.RejectsInvalidWorkerCount`). All 63
pass under: plain Debug build, the `-Wall -Wextra -Wpedantic -Wshadow`
build (0 warnings), ASan+UBSan, and (the 19 concurrency-relevant ones)
ThreadSanitizer.

## 6. What this pass deliberately did not do

- It did not add tests purely to inflate the count — every addition
  above is tied to a specific scenario (degenerate mesh, an orthogonal
  numerical regime, an unguarded function argument) that the existing
  suite genuinely didn't cover, not a restatement of an existing test.
- It did not go back and silently "fix" the two earlier, already-disclosed
  Windows CI failures (the CMake generator mismatch, the `interface`/COM
  macro clash) — those remain documented in `docs/packaging/windows.md`
  and `docs/architecture/hardware_mocks.md` exactly as they happened.
- Ad hoc sanitizer build directories (`build_warn/`, `build_asan/`,
  `build_tsan/`) are gitignored (`backend/build*/`) and not committed;
  they're reproducible from the commands above, not submission artifacts.
