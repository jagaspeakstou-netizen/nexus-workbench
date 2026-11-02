# Technical Appendix

Supporting detail for [`REPORT.md`](REPORT.md). Everything here is
mechanically derived from the repository itself (test file contents,
committed CSVs, linked CI runs) — regenerate any of it with the commands
in §A.4 below rather than trusting this document's numbers to stay fresh
forever.

## A.1 Test inventory (63 tests, by file)

| File | Module | `TEST()` count |
|---|---|---|
| `test_geometry.cpp` | Geometry Lab | 22 |
| `test_performance.cpp` | Performance Lab | 12 |
| `test_hardware_mocks.cpp` | Hardware Integration | 11 |
| `test_thermal.cpp` | Thermal Analysis | 10 |
| `test_neural.cpp` | Neural Computation | 6 |
| `test_status.cpp` | Core/status | 2 |
| **Total** | | **63** |

Counted directly via `grep -c "^TEST(" backend/tests/*.cpp` — not asserted
from memory.

## A.2 CI runs referenced in this report

| Run | Commit | Result | What it proved |
|---|---|---|---|
| [41338201277](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41338201277) | `75ed99a` | Failed | CMake generator mismatch against that runner's actual installed VS version |
| [41338521566](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41338521566) | `4c6eaf6` | Passed | 43/43 tests, `Launch.exe` live 6s post-launch, static WebView2 loader |
| (hardware module run) | `dec8171` | Failed | `interface`/COM macro field-name clash under MSVC |
| (hardware module run) | `11138f1` | Passed | 60/60 tests on real Windows after the fix |
| [41373581581](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41373581581) | `01a865e` | Passed | 63/63 tests on real Windows, including the Day 11 validation additions |

Workflow source: [`.github/workflows/windows-build.yml`](../../.github/workflows/windows-build.yml).

## A.3 Evidence files (committed)

Raw CSVs — `results/csv/`:
- `thermal_n{10,20,40,80}.csv`, `thermal_convergence.csv`
- `neural_training.csv`, `neural_gradient_check.csv`
- `hardware_pinout_{microcontroller,fpga,jtag}.csv`
- `hardware_telemetry_{mcu,fpga,jtag}-sim-{01,02}.csv`

Benchmark CSVs — `results/benchmarks/`:
- `performance_summary.csv`, `performance_timings.csv`, `performance_hardware.csv`

Labelled plots (generated from the above by `scripts/generate_plots.py`) — `results/plots/`:
- `thermal_profile_and_midpoint_error_n40.png`
- `thermal_mesh_convergence.png`
- `neural_training_loss.png`
- `neural_gradient_check.png`
- `performance_speedup_efficiency.png`
- `hardware_telemetry_dropout_recovery.png`

End-to-end UI check screenshots (real Playwright run against the built
frontend + live backend, see [`E2E_CHECK.md`](E2E_CHECK.md)) — `e2e_evidence/`:
- `overview.png`, `geometry.png`, `thermal.png`, `performance.png`, `neural.png`, `hardware.png`, `validation.png`, `docs.png` (cold-load, every sidebar page)
- `geometry_after_stl_load.png`, `geometry_after_csv_load.png` (after real "Load sample" clicks)

## A.4 Full reproduction commands

```bash
# 1. Build (Linux/macOS; see docs/packaging/windows.md for Windows)
cd backend
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 2. Run the full test suite
cd build && ctest --output-on-failure   # expect 63/63

# 3. Regenerate raw CSV evidence
./nexus_thermal_export ../../results/csv
./nexus_neural_export ../../results/csv
./nexus_hardware_export ../../results/csv
./nexus_performance_export ../../results/benchmarks

# 4. Regenerate labelled plots from that evidence
cd ../.. && python3 scripts/generate_plots.py

# 5. Validation builds (see docs/architecture/validation.md for full detail)
cd backend
cmake -S . -B build_warn -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Wshadow"
cmake --build build_warn -j            # expect 0 warnings

cmake -S . -B build_asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build_asan -j
cd build_asan && ASAN_OPTIONS=detect_leaks=1 ctest --output-on-failure  # expect 63/63 clean

# 6. Frontend (dev mode)
cd ../../frontend
npm install && npm run dev

# 7. End-to-end UI check (real backend + real built frontend; see E2E_CHECK.md)
cd ../backend && cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release
cmake --build build_release -j && ./build_release/nexus_server 8787 &
cd ../frontend && npm run build && npx vite preview --port 4173 --strictPort &
pip install --break-system-packages playwright && playwright install chromium
cd .. && python3 scripts/e2e_check.py && python3 scripts/e2e_interactive.py
```

## A.5 Module → architecture doc map

| Module | Doc |
|---|---|
| Geometry Lab | `docs/architecture/geometry_lab.md` |
| Thermal Analysis | `docs/architecture/thermal_fea.md` |
| Performance Lab | `docs/architecture/performance_lab.md` |
| Neural Computation | `docs/architecture/backprop.md` |
| Hardware Integration | `docs/architecture/hardware_mocks.md` |
| Validation (Day 11) | `docs/architecture/validation.md` |
| Windows packaging | `docs/packaging/windows.md` |
