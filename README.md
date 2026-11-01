# NEXUS — Engineering Intelligence Workbench

> One workspace. Multiple engineering disciplines. Verified computation.

PASS Science Works R&D Internship 2026–2027 — Unified Engineering Workbench
technical assignment (Web & Software Development track).

## Status

Implemented and verified: **Geometry Lab**, **Thermal Analysis**,
**Performance Lab**, **Neural Computation** (plus the API/core walking
skeleton and Overview page). Not yet implemented: Hardware Integration,
Validation pass, Documentation — all honestly reported as pending in the
UI itself, not stubbed out silently. See `/api/status` for the live,
code-driven source of this list (`backend/src/core/status.cpp`), which is
what the Overview page actually renders.

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

`Launch.exe` — a native WebView2 shell (`windows/launcher/`) that spawns
`nexus_server.exe` and displays the built frontend, no terminal/admin/
internet/install step required on a clean Windows 11 machine. Built and
verified on a real `windows-latest` GitHub Actions runner
(`.github/workflows/windows-build.yml`). Full packaging details, the
clean-machine checklist, and the actual build/launch evidence are in
[`docs/packaging/windows.md`](docs/packaging/windows.md).

## Editor-based run (VS Code / Antigravity)

`.vscode/tasks.json` and `.vscode/launch.json` provide one-click build/run/
debug tasks for the backend and frontend from inside the editor — see
[`docs/packaging/windows.md`](docs/packaging/windows.md#editor-based-source-build-vs-code-or-antigravity)
for the extension prerequisites. This path is independent of `Launch.exe`.
