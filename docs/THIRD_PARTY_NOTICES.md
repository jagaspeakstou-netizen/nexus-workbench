# Third-Party Notices

This project vendors or depends on the following third-party software.
None of it has been modified from its published form unless noted.

## Vendored in-tree (`backend/third_party/`)

- **cpp-httplib** (`backend/third_party/httplib/`) — MIT License.
  https://github.com/yhirose/cpp-httplib
- **nlohmann/json** (`backend/third_party/json/`) — MIT License.
  https://github.com/nlohmann/json

Both are used unmodified as single-header libraries, per their own
documented integration method.

## Fetched via package manager (not vendored in-tree; see lock files)

- **GoogleTest / GoogleMock** — BSD-3-Clause License. Resolved via
  vcpkg; see `backend/vcpkg.json` for the pinned port version.
- **React**, **React DOM** — MIT License.
- **Vite** — MIT License.
- **Tailwind CSS** — MIT License.
- **TypeScript** — Apache License 2.0.
- Transitive npm dependencies — see `frontend/package-lock.json` for
  the full resolved tree; each package's own license applies and is
  not individually reproduced here (standard practice for an npm
  project of this size — the lock file is the authoritative record of
  what's installed and at what version).

## Windows launcher

- **Microsoft Edge WebView2 SDK** (NuGet package, `windows/launcher/`)
  — SDK is MIT-licensed; the WebView2 *runtime* itself (present on
  modern Windows by default, or redistributed by Microsoft) is
  governed by Microsoft's own license terms, not this project's.

## Disclosure of AI-generated content

Code, documentation, and test content produced with AI assistance
(Claude, via Claude Code) is disclosed in full in
[`technical_report/AI_USE_LOG.md`](technical_report/AI_USE_LOG.md),
per the ownership statement in [`OWNERSHIP.md`](OWNERSHIP.md). AI
assistance is not a "third party" in the licensing sense above — no
license obligation attaches to it — but is disclosed here for
completeness since the assignment brief asks for AI-assistance
disclosure alongside dependency/license disclosure.

## No warranty

This is an internship technical-assignment submission, not a
distributed product; no license file for the project's own original
code is included beyond this disclosure, since the submission is not
being redistributed.
