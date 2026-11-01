# Windows packaging

Two independent ways to run NEXUS on Windows 11, matching the assignment's
requirement that they not depend on each other:

1. **Standalone launcher** (`Launch.exe`) — for someone who just wants to
   run the app. No terminal, no PowerShell, no admin rights, no package
   installs, no first-run build, no internet connection, no editor.
2. **Source build in an editor** (VS Code or Antigravity) — for a reviewer
   who wants to read, modify, and re-run the code. Documented separately
   below; the CMake instructions it uses are the *same* ones used for the
   Linux dev build (`backend/CMakeLists.txt`), with the Windows-only
   `nexus_launcher` target (`windows/launcher/`) added automatically when
   `WIN32` is set — see the guard in `backend/CMakeLists.txt`.

## How the evidence in this document was produced

**Status: pending first CI run.** This document, the launcher source
(`windows/launcher/Launcher.cpp`), the CMake target
(`windows/launcher/CMakeLists.txt`), and the CI workflow
(`.github/workflows/windows-build.yml`) were all authored on this project's
Linux dev container, which cannot compile or run Windows executables. Per
this project's own rule against fabricating evidence, nothing below is
claimed as "verified" until `.github/workflows/windows-build.yml` has
actually been run on a real `windows-latest` GitHub Actions runner and
produced:

- a build log showing the CMake configure + MSVC compile + `ctest` run,
  including the actual GoogleTest pass/fail counts on Windows;
- `windows_launch_screenshot.png` — a screen capture taken by the runner
  itself, 6 seconds after it started `Launch.exe`, with the workflow step
  explicitly failing (not silently continuing) if the process had already
  exited by that point;
- `launch_status.txt` — the process's own state (`Get-Process`) at
  screenshot time;
- `NEXUS_Windows_x64.zip` — the exact packaged output folder the workflow
  compressed, available to download and test on a clean machine.

Once that workflow has run, this section is replaced with: the run's URL,
the date, the actual logical-core/compiler/version strings MSVC reported
(the same "disclose actual hardware/build info" principle the Performance
Lab module already follows on Linux), and the real pass/fail test count —
not restated from this draft, but copied from that run's own output.

**What is already true without that run:** the CMake build graph is
correct cross-platform structure (the `if(WIN32)` guard was tested by
confirming it is *not* entered on this Linux container — `cmake --build`
here still only produces the Linux targets, unchanged). What is NOT yet
verified is whether `Launcher.cpp` compiles against the real WebView2 SDK
headers/libs, since that SDK only exists on Windows. That is exactly what
the first CI run checks, and if it fails, the fix happens there and is
disclosed here, not hidden.

## Architecture

```
Launch.exe  (windows/launcher/, WIN32 WebView2 shell)
  ├─ spawns →  nexus_server.exe   (same backend binary as Linux, binds 127.0.0.1:8787)
  └─ hosts  →  WebView2 control, navigating to frontend/index.html (built Vite SPA,
               copied next to Launch.exe at build time) which calls the backend
               exactly as it does in local dev (frontend/src/services/api.ts)
```

Everything a reviewer unzips onto a clean Windows 11 machine is one folder:

```
NEXUS_Windows_x64/
  Launch.exe
  nexus_server.exe
  WebView2Loader.dll        (only present if the static loader lib wasn't
                              available for the restored SDK version — see
                              windows/launcher/CMakeLists.txt)
  frontend/                 (built Vite SPA: index.html, assets/*)
```

## Why no VC++ Redistributable, and what WebView2 requires

- The MSVC runtime is **statically linked**
  (`CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded` in
  `windows/launcher/CMakeLists.txt`), so `Launch.exe` and `nexus_server.exe`
  carry their own C/C++ runtime and need no `vc_redist.x64.exe` install
  step.
- **Microsoft Edge WebView2 Runtime** is the one OS-level dependency this
  project does not bundle, because Windows 11 ships it pre-installed by
  default (the "Evergreen" runtime, serviced by Windows Update — this is a
  documented Microsoft platform guarantee for Windows 11, not an assumption
  specific to this project). If it is somehow missing, `Launch.exe` shows a
  message box with a direct download link rather than crashing silently
  (`ShowWebView2MissingError()` in `Launcher.cpp`).

## Clean-machine verification checklist

To be run on an actual Windows 11 machine (the CI runner counts; so does a
reviewer's own PC) with none of this project's dev tools installed:

- [ ] Unzip `NEXUS_Windows_x64.zip` to a fresh folder (anywhere — Desktop,
      a USB drive, `C:\Temp`; the app does not assume an install location).
- [ ] Double-click `Launch.exe`. No other action.
- [ ] A window titled "NEXUS Engineering Intelligence Workbench" opens
      within a few seconds, showing the Overview page with live module
      status (not a blank page, not an error page — if the backend hasn't
      finished binding its port yet, the Thermal/Geometry/Performance
      pages show the app's own honest "Backend unreachable" message with a
      retry action, which is a correct render, not a failure).
  - [ ] Click into Thermal Analysis, Geometry Lab, and Performance Lab —
        each should load real data from `nexus_server.exe` (not placeholder
        text).
- [ ] Close the window. Confirm in Task Manager that `nexus_server.exe` is
      no longer running (the launcher terminates it on `WM_DESTROY`).
- [ ] Confirm no install dialog, no admin/UAC elevation prompt, and no
      internet access was required at any point (disconnect networking
      first, if you want to check this directly — the app is fully local
      after the one-time WebView2-runtime check).

## Editor-based source build (VS Code or Antigravity)

Independent of `Launch.exe` — a reviewer never needs to run the launcher
to read, build, or modify the source.

**Prerequisites:**
- Visual Studio 2022 (Community is fine) with the "Desktop development
  with C++" workload, or the standalone Build Tools + Windows SDK.
- [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
  and [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
  extensions (VS Code); the equivalent bundled extensions in Antigravity.
- Node.js 20+ (for the frontend).
- `vcpkg` for GoogleTest on Windows (GitHub's own `windows-latest` runner
  ships this pre-installed at `%VCPKG_INSTALLATION_ROOT%`; on a personal
  machine, install it per <https://vcpkg.io> and set that environment
  variable, or pass `-DCMAKE_TOOLCHAIN_FILE=<path>\vcpkg.cmake` directly).
  `backend/vcpkg.json` declares the `gtest` dependency in manifest mode, so
  no manual `vcpkg install` step is needed once the toolchain file is set.

**Steps:**
1. Open the repository root folder in VS Code / Antigravity.
2. Run the task **"NEXUS: Install frontend deps"**, then
   **"NEXUS: Build backend"** (`.vscode/tasks.json` — these also appear in
   the Command Palette under "Tasks: Run Task", and the CMake Tools
   extension's own status bar can drive the same `backend/CMakeLists.txt`
   directly if preferred).
3. Run **"NEXUS: Run full stack (backend + frontend)"** to start both dev
   servers, or use the **"NEXUS: Debug backend (nexus_server)"** launch
   configuration (`.vscode/launch.json`) to step through the C++ backend
   under the debugger.
4. Open `http://localhost:5173` in a browser for the frontend dev server
   (hot-reloading), which talks to the backend on `:8787` exactly as
   `Launch.exe`'s bundled frontend does.

This path never touches `windows/launcher/` — the `nexus_launcher` /
`Launch.exe` CMake target only activates when CMake itself is configured
with `WIN32` set, i.e. targeting an actual Windows build, so a Linux/macOS
dev machine building the backend for local development is unaffected.

## Reproducing the CI run yourself

```
# from the repository root, with a GitHub remote configured
git push origin main
# then, in the GitHub UI: Actions tab -> windows-build -> Run workflow
# (also runs automatically on every push to main/master)
```

The workflow file is `.github/workflows/windows-build.yml`. Its artifacts
(`nexus-windows-evidence`) are downloadable from the run's Summary page for
30 days by default; this project's actual submission copies the screenshot,
status file, and a trimmed build log excerpt into this document once a run
has completed, with a link to the run itself.
