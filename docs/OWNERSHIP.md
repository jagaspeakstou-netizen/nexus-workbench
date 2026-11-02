# Ownership Statement

Submission: NEXUS Engineering Intelligence Workbench
Candidate: Gaayathri Muralidharan
PASS Science Works R&D Internship 2026-2027 — Web & Software Development track

## Original work (candidate-directed, AI-assisted implementation)

All source code under `backend/src/`, `backend/include/`, `backend/tests/`,
`frontend/src/`, `windows/launcher/`, and all content under `docs/`,
`scripts/`, and `data/*/README.md` was produced for this submission
specifically, directed by the candidate and generated with AI assistance
(Claude, via Claude Code) as disclosed in full in
[`docs/technical_report/AI_USE_LOG.md`](technical_report/AI_USE_LOG.md).
This includes:

- The six engineering modules (Geometry, Thermal, Performance, Neural,
  Hardware, Validation) and their HTTP API layer.
- The React/TypeScript frontend (all pages and components).
- The GoogleTest suite (63 tests).
- The Windows WebView2 launcher (`Launch.exe` source).
- The GitHub Actions CI workflow.
- All documentation: architecture notes, technical report, AI-use log,
  appendix, and the AI challenge test record.
- Sample/synthetic input data under `data/` (the STL tetrahedron and the
  synthetic ANSYS-format CSV are self-authored/generated for this
  project, not sourced from a third party — see each `data/*/README.md`
  for provenance).

No part of this submission's own engineering logic (solver algorithms,
API design, UI structure, test design) was copied from an external
codebase, tutorial, or prior project. Where a design choice follows a
standard, well-known method (e.g., the Thomas algorithm for tridiagonal
systems, the Galerkin finite-element formulation, backpropagation via the
chain rule), that is disclosed in the architecture docs as the standard
method it is, not claimed as a novel contribution.

## Reused work (third-party dependencies)

This submission depends on the following third-party libraries, used
as-is via their published APIs, not modified:

| Dependency | Role | License | Vendored at |
|---|---|---|---|
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | HTTP server (backend) | MIT | `backend/third_party/httplib/` |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON (backend) | MIT | `backend/third_party/json/` |
| [GoogleTest](https://github.com/google/googletest) | Unit testing | BSD-3-Clause | via vcpkg (`backend/vcpkg.json`) |
| React | Frontend UI framework | MIT | via npm (`frontend/package-lock.json`) |
| Vite | Frontend build tool | MIT | via npm |
| Tailwind CSS | Frontend styling | MIT | via npm |
| TypeScript | Frontend language | Apache-2.0 | via npm |
| Microsoft WebView2 SDK | Windows launcher's embedded browser control | MIT (SDK); WebView2 runtime itself under Microsoft's own EULA | via NuGet (`windows/launcher/`) |

Full license texts and the complete transitive dependency list are not
reproduced here; see [`docs/THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
for the license-disclosure summary, and `backend/vcpkg.json` /
`frontend/package-lock.json` for exact pinned versions.

## Handwritten appendix (Task F)

The handwritten forward-pass/backprop/gradient-descent derivation
required by Task F is a physical artifact (scanned/photographed by the
candidate) and is **not** part of the AI-assisted work described above;
see [`docs/technical_report/REPORT.md`](technical_report/REPORT.md) for
its status in this submission.
