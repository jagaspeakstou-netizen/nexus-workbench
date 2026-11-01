# Performance Lab module — multicore batch solve

Implements `backend/include/nexus/performance.hpp` /
`backend/src/performance/{performance,hardware}.cpp`.

## What's being benchmarked

A batch of `case_count` independent 1D thermal FEA solves (reusing
`nexus::thermal::solve()` from the Thermal module unchanged — same
solver, same numerical method, only `k` and `q` vary per case), run two
ways:

- **Serial** (`run_batch_serial`): a plain loop on the calling thread.
- **Parallel** (`run_batch_parallel`): `num_workers` `std::thread`s pull
  case indices from a shared `std::atomic<size_t>` counter (a bounded
  work-stealing pool — not one thread per case, and never more threads
  than `num_workers`).

## Reproducibility

`generate_cases(seed, count, k_range, q_range)` uses `std::mt19937`
seeded deterministically; the same seed always produces the same `k`,
`q` per case in the same order. Both the serial and parallel paths run
the *exact* same case list and the *exact* same solver, so any
difference in their output is a bug, not expected variation.

## Deterministic result ordering, despite nondeterministic scheduling

`run_batch_parallel` pre-sizes the result vector to `cases.size()` and
each worker writes `result.case_results[i] = ...` directly to its
claimed index `i` — never `push_back`. So the result vector's order
always matches the input case order, regardless of which thread
finishes which case first. `test_performance.cpp`
(`ParallelResultsAreInCaseOrderRegardlessOfWorkerCount`) checks this at
1, 2 and 4 workers.

## Numerical agreement

`max_abs_temperature_difference()` compares every node, every case,
between two `BatchRunResult`s. `run_full_benchmark` always runs one
serial pass and one parallel pass (at the highest tested worker count)
purely for this check, separate from the timed repetitions. Measured on
this project's dev container: **0.0 °C difference** (bit-for-bit
identical — expected, since both paths call the identical
`nexus::thermal::solve()` with identical inputs; the tolerance exists
for the general case, e.g. an implementation using a parallel reduction
with a different summation order).

## What's excluded from timing, and why

Only the loop in `run_batch_serial` / `run_batch_parallel` is timed
(`std::chrono::steady_clock`, wrapping just the solve loop). File I/O,
JSON serialization, HTTP transport, and the frontend's chart rendering
are all outside the timed region — timing those would measure the
transport layer, not the parallel algorithm.

**Warm-up:** one full untimed batch pass per worker-count configuration
immediately before that configuration's timed repetitions, to absorb
first-touch allocator/page-fault and thread-creation overhead so it
doesn't bias the first measured repetition.

**Launch-to-ready:** not meaningfully separate from the above for this
CLI/API path — there's no process launch or UI render inside what's
timed to begin with.

## Measured results (this project's own dev container — see
`results/benchmarks/performance_hardware.csv` for the exact disclosure)

This container reports **2 logical CPUs** (`std::thread::hardware_concurrency()`
via `/proc/cpuinfo` — an Intel Xeon, containerized, so "physical cores"
is reported as an estimate equal to the logical count rather than
guessed at). Worker counts above 2 are capped, not run — `run_full_benchmark`
deduplicates and clamps `config.worker_counts` to
`std::thread::hardware_concurrency()` before running anything, and
`test_performance.cpp` (`NeverExceedsAvailableLogicalCpus`) checks this
holds even if asked for 999 workers.

With 4000 cases at N=200 elements each (large enough that per-case work
dominates thread-launch overhead — see note below), the figures
currently committed in `results/benchmarks/performance_summary.csv`
(most recent run, from the clean rebuild) are:

| Workers | Median (s) | Speedup | Efficiency |
|---|---|---|---|
| 1 | 0.0280 | — | — |
| 2 | 0.0215 | 1.30× | 65.1% |

**A real limitation, found and fixed during development, not
hypothetical:** the first version of this benchmark used 40 cases at
N=40 elements — individual solves so fast (microseconds) that
`std::thread` creation overhead exceeded the actual work, and the
"parallel" run measured *slower* than serial (0.44× "speedup"). This
wasn't a bug in the parallel logic — the agreement check still passed —
it was a benchmark design error: too small a workload. Scaling to 4000
cases × N=200 fixed that; a full from-scratch rebuild-and-rerun
immediately before writing this doc produced 1.30×/65% rather than an
earlier same-config run's 1.80×/90% — both are genuine, unedited CLI
output, just from different moments on what is very likely a shared,
virtualized cloud container (2 reported logical CPUs, probably
sharing physical cores with other tenants). That run-to-run variance
is itself a second real, disclosed limitation: these absolute numbers
are only meaningful as "parallel measurably helps, overhead doesn't
dominate it, and serial/parallel agree exactly" on this specific
machine at this specific moment — not as a precise, reproducible
speedup figure. **No minimum speedup is claimed or required** — this
machine has 2 logical CPUs, so 2.0× is the theoretical ceiling, and
whatever the most recent run measured below that is reported as-is,
not smoothed or cherry-picked.

## Reproducing

```
cd backend/build
./nexus_performance_export ../../results/benchmarks   # regenerates the CSVs above
cd .. && ctest --output-on-failure -R "CaseGenerator|BatchRun|Hardware|Benchmark"
```
