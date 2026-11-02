# Demonstration Walkthrough (timestamped screenshots + logs)

This is the §8-required demonstration, in the form the brief explicitly
allows as an alternative to a video: **a timestamped screenshot
walkthrough with logs.** Every timestamp below and every screenshot in
[`e2e_evidence/`](e2e_evidence/) comes from one single, real,
uninterrupted run — not reconstructed afterward. The raw log is
[`e2e_evidence/run_log.txt`](e2e_evidence/run_log.txt).

## What this run proves

A clean build of the real C++ backend, a real production frontend
build served statically (not a dev server), driven by real Playwright
browser automation clicking through every page and several interactive
flows — with the exact wall-clock time of each step and zero console
or page errors throughout.

## Step-by-step, with real timestamps

| Time (UTC) | Step | Evidence |
|---|---|---|
| `02:36:28` | Backend (Release) build starts and completes | `run_log.txt` lines 1-9 — all 7 CMake targets built |
| `02:36:28` | `nexus_server` started on port 8787 | `run_log.txt` |
| `02:36:29` | `/api/status` responds with live module data | `run_log.txt` — real JSON excerpt captured |
| `02:36:29`–`02:36:54` | Frontend production build (`tsc -b && vite build`) | `run_log.txt` — real Vite build output, 52 modules |
| `02:36:54` | `vite preview` serves the built app on port 4173 | `run_log.txt` |
| `02:36:56`–`02:37:04` | **Cold-load check**: every sidebar page visited fresh, screenshotted | see table below |
| `02:37:04`–`02:37:11` | **Interactive check**: real button clicks produce real changed output | see table below |
| `02:37:11` | Server and preview shut down cleanly | `run_log.txt` |

Total wall-clock time for the whole walkthrough: **43 seconds**
(`02:36:28` → `02:37:11`), end to end, from a cold build to a fully
exercised UI.

## Cold-load screenshots (one per sidebar page, this run)

All captured between `02:36:58` and `02:37:04` in this same run (file
`mtime` on each PNG matches):

| Page | Screenshot | Console errors | Page errors | Backend unreachable |
|---|---|---|---|---|
| Overview | [`overview.png`](e2e_evidence/overview.png) | 0 | 0 | No |
| Geometry Lab | [`geometry.png`](e2e_evidence/geometry.png) | 0 | 0 | No |
| Thermal Analysis | [`thermal.png`](e2e_evidence/thermal.png) | 0 | 0 | No |
| Performance Lab | [`performance.png`](e2e_evidence/performance.png) | 0 | 0 | No |
| Neural Computation | [`neural.png`](e2e_evidence/neural.png) | 0 | 0 | No |
| Hardware Integration | [`hardware.png`](e2e_evidence/hardware.png) | 0 | 0 | No |
| Validation & Tests | [`validation.png`](e2e_evidence/validation.png) | 0 | 0 | No |
| Documentation | [`docs.png`](e2e_evidence/docs.png) | 0 | 0 | No |

## Interactive-flow screenshots (real button clicks, real state change)

| Action | Result | Screenshot |
|---|---|---|
| Click "Load sample (self-authored tetrahedron)" | STL renders, placeholder text gone | [`geometry_after_stl_load.png`](e2e_evidence/geometry_after_stl_load.png) |
| Click "Load sample (synthetic)" (CSV) | Point cloud renders, placeholder text gone | [`geometry_after_csv_load.png`](e2e_evidence/geometry_after_csv_load.png) |
| Click "Re-run training" (Neural) | New "Final y_hat" value appears | `run_log.txt` — `Neural 're-run training' produced a result: True` |
| Click "Re-run sequence" (Hardware) | "CONNECTED" state appears | `run_log.txt` — `Hardware 're-run sequence' produced a result: True` |

Zero console or page errors across all four interactive actions
(`run_log.txt`: `console/page errors during interactive flow: 0`).

## Reproducing this exact walkthrough

```bash
cd backend && cmake --build build_release -j
./build_release/nexus_server 8787 &
cd ../frontend && npm run build && npx vite preview --port 4173 --strictPort &
cd .. && python3 scripts/e2e_check.py && python3 scripts/e2e_interactive.py
```

See [`E2E_CHECK.md`](E2E_CHECK.md) for the fuller writeup of this check
(methodology, scope, and the stale-status UI bug this same check found
and fixed earlier in the project).
