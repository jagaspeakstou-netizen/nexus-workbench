# NEXUS — Engineering Intelligence Workbench

> One workspace. Multiple engineering disciplines. Verified computation.

PASS Science Works R&D Internship 2026–2027 — Unified Engineering Workbench
technical assignment (Web & Software Development track).

## Status

**Phase 1 of 5 — walking skeleton.** Only the API/core module and the
Overview page are implemented. Every other module (Geometry, Thermal,
Performance, Hardware, Neural, Validation, Documentation) is honestly
reported as pending in the UI itself — see `/api/status`.

## Prerequisites (source build)

- CMake ≥ 3.16, a C++17 compiler (tested with GCC 13)
- GoogleTest (`apt install libgtest-dev` on Debian/Ubuntu; vcpkg `gtest` port on Windows)
- Node.js ≥ 18, npm

## Running from source

```bash
# Backend
cd backend
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/nexus_server 8787      # serves http://127.0.0.1:8787/api/status

# Backend tests
cd backend/build && ctest --output-on-failure

# Frontend (separate terminal)
cd frontend
npm install
npm run dev                     # serves http://127.0.0.1:5173
```

Open `http://127.0.0.1:5173` with the backend running — the Overview page
fetches live module status from the C++ backend; it shows an explicit
"backend unreachable" error if `nexus_server` isn't running, rather than
silently showing stale or fake data.

## Architecture

- `backend/` — C++17 core (`nexus_core`) + a thin HTTP/JSON transport
  layer (`nexus_server`, via vendored `cpp-httplib` + `nlohmann/json`
  under `backend/third_party/`). All computation lives in `nexus_core`;
  `src/api/main.cpp` only wires HTTP routes to it.
- `frontend/` — React + TypeScript + Vite + Tailwind. `src/services/api.ts`
  is the single place that knows the backend's URL and JSON shape
  (mirrored in `src/types/status.ts`).
- `data/`, `results/` — sample inputs and generated outputs for later
  phases (geometry files, thermal CSVs, benchmark results).
- `docs/` — architecture notes, the technical report, and the AI-use log.

See the full requirements-to-rubric mapping and 15-day schedule in the
companion planning doc (Phase 1 deliverable, shared separately).

## One-click Windows launch

Not yet built — planned for Day 12 (GitHub Actions `windows-latest`
runner + WebView2 shell). Source-build instructions above are the only
supported path until then.
