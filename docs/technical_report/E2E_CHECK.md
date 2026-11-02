# End-to-end UI check

Real browser automation (Playwright/Chromium) driving the **built**
frontend against the **live, real** C++ backend — not a mock, not a
dev-server hot-reload session. This is the item the Day 11 writeup had
flagged as not yet run; it has now actually been run, with the results
below.

## How it was run

```bash
# 1. Real backend, release build, real port
cd backend && cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release
cmake --build build_release -j
./build_release/nexus_server 8787 &

# 2. Real production frontend build, served statically (not `npm run dev`)
cd frontend && npm run build
npx vite preview --port 4173 --strictPort &

# 3. Drive it with Playwright
python3 scripts/e2e_check.py          # every sidebar page, cold load
python3 scripts/e2e_interactive.py    # real button clicks (Load sample, Re-run training, Re-run sequence)
```

Both scripts fail loudly (non-zero exit) on any JS console error, page
error, or a "Backend unreachable" state — they don't just check that a
page didn't crash the test runner.

## Results

### Cold-load check (`scripts/e2e_check.py`) — every sidebar page

| Page | Console errors | Page errors | Backend unreachable | Result |
|---|---|---|---|---|
| Overview | 0 | 0 | No | PASS |
| Geometry Lab | 0 | 0 | No | PASS |
| Thermal Analysis | 0 | 0 | No | PASS |
| Performance Lab | 0 | 0 | No | PASS |
| Neural Computation | 0 | 0 | No | PASS |
| Hardware Integration | 0 | 0 | No | PASS |
| Validation & Tests | 0 | 0 | No | PASS |
| Documentation | 0 | 0 | No | PASS |

**8/8 pages pass**, all against the real backend — screenshots for every
page are committed under
[`e2e_evidence/`](e2e_evidence/) (e.g.
[`e2e_evidence/thermal.png`](e2e_evidence/thermal.png),
[`e2e_evidence/neural.png`](e2e_evidence/neural.png),
[`e2e_evidence/hardware.png`](e2e_evidence/hardware.png)).

### A real bug this check found and fixed

Before this check was run, `App.tsx` hardcoded the "Validation & Tests"
and "Documentation" sidebar pages to always show a static "not
implemented yet" message, regardless of what the backend actually
reported — a leftover from before those two modules existed. Once Day
11/14 made both of them genuinely implemented, that hardcoded page
became a stale, false claim that would have shipped unnoticed.

Fixed by replacing it with `ModuleStatusPage` (`frontend/src/pages/ModuleStatusPage.tsx`),
which fetches the same live `/api/status` the Overview page already
uses and renders whatever that module's entry actually says — so it
cannot drift out of sync with the backend again. Confirmed in the
screenshots: [`e2e_evidence/validation.png`](e2e_evidence/validation.png) now shows "Implemented &
tested" live from the backend, and
[`e2e_evidence/docs.png`](e2e_evidence/docs.png) shows "Implemented, untested" with the real note
text about the AI-use log — not a hardcoded placeholder.

### Interactive-flow check (`scripts/e2e_interactive.py`) — real clicks, not just page loads

| Action | Result |
|---|---|
| Geometry Lab → "Load sample" (STL) | STL parsed by the real backend, rendered as a real Three.js mesh ([`e2e_evidence/geometry_after_stl_load.png`](e2e_evidence/geometry_after_stl_load.png)) |
| Geometry Lab → "Load sample" (CSV) | CSV parsed and rendered ([`e2e_evidence/geometry_after_csv_load.png`](e2e_evidence/geometry_after_csv_load.png)) |
| Neural Computation → "Re-run training" | Produced a fresh "Final y_hat" result |
| Hardware Integration → "Re-run sequence" | Produced fresh per-device "CONNECTED" results |

**0 console/page errors across the entire interactive flow.**

## What this check does, and doesn't, cover

- It confirms every page loads real data from a real running backend
  with no JS errors — the thing that was genuinely missing before now.
- It does not replace the 63 backend unit tests (which test the
  computation itself, independent of the UI) — it tests the wiring
  between frontend and backend, which unit tests can't.
- It was run once, interactively, in this environment — it is not wired
  into CI as an automated gate (a real remaining gap, not hidden: adding
  it to `.github/workflows/` as its own job would be the natural next
  step, not attempted here due to time).
