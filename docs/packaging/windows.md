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

## A real defect found and fixed (2026-11-02)

**This section is itself evidence, not a claim.** The CI run referenced
below as "Run #2 — passed, every step" never actually had its own
screenshot artifact inspected until the candidate personally downloaded
and opened it on 2026-11-02 — which revealed that `Launch.exe` had in
fact hit its own `ShowWebView2MissingError()` dialog ("NEXUS — Startup
Error: NEXUS could not start the Microsoft Edge WebView2 component"),
not a working application window. The CI gate at the time only checked
`-not $proc.HasExited` ("is the process still alive"), which is true even
when a modal error dialog is showing — its message loop keeps pumping,
so the process is technically "responding" while the app has not
actually started.

**Round 1 theory — tested and disproven:** `windows-latest` runners are
Windows *Server* images (confirmed from that screenshot's own desktop
watermark: "Windows Server 2025 Datacenter"), not literal Windows 11
client, and Microsoft's "WebView2 ships pre-installed" guarantee applies
specifically to Windows 11 client. The first fix installed the WebView2
Evergreen Runtime in CI and added a `FindWindow`-based check for the
error dialog. A fresh CI run went green — but the candidate pulled the
new screenshot anyway and **the exact same startup-error dialog was
still showing**. The run's own log showed the WebView2 Runtime was
already present (v155.0.4283.62) before the install step even ran, so
"runtime missing" was never the real cause. The hardened detection had
its own bug too: the exact-title `FindWindow` match silently failed
(see below), so CI reported "success" a second time despite the launch
still failing — caught, again, only by personally looking at the
screenshot.

**Round 2 — diagnostics instead of a third guess** (pushed 2026-11-02):
1. `windows/launcher/Launcher.cpp` now logs the actual `HRESULT` and
   system error text from the failing WebView2 call to
   `launcher_error.log` beside `Launch.exe`
   (`LogStartupFailure()`) — the next run will show the real error code
   instead of another theory, and CI copies/inlines that file into the
   run's evidence.
2. The detection bug is fixed properly: exact-title `FindWindow` is
   replaced with `EnumWindows` + a substring check for `"Startup Error"`
   across every top-level window title, immune to the exact-string
   mismatch that broke the first attempt.
3. That mismatch had its own cause, fixed too: the dialog's title
   contains an em-dash written as a literal UTF-8 byte sequence in the
   `.cpp` source, which MSVC — without an explicit source-charset flag —
   mis-decoded into mojibake (visible in both captured screenshots as
   `â€"` instead of `—`). Fixed via a `—` escape sequence plus an
   explicit `/utf-8` flag in `windows/launcher/CMakeLists.txt`.
4. The WebView2 Runtime install step from round 1 is kept (harmless,
   usually a no-op) but is no longer claimed as *the* fix.

This is disclosed here, in `docs/technical_report/AI_USE_LOG.md` Item 8,
and in the technical report — two real, consecutive detection failures,
corrected rather than hidden, rather than silently re-running CI and
only keeping a "VERIFIED" claim once it happens to go green.

**Round 3 — the real root cause, from the first real diagnostic run:**
the round-2 fixes worked exactly as intended on the next CI run: the
window title rendered correctly, the substring detection correctly
caught the still-showing error dialog (and correctly failed the job
instead of reporting a false pass), and — for the first time —
`launcher_error.log`'s content was actually readable, via the
`Write-Host` mirroring added alongside it. It read:

```
[CreateCoreWebView2EnvironmentWithOptions] HRESULT=0x800401f0 (CoInitialize has not been called.)
```

`CO_E_NOTINITIALIZED`. `windows/launcher/Launcher.cpp` calls
`CreateCoreWebView2EnvironmentWithOptions`, which is COM-based, but the
launcher never called `CoInitializeEx` anywhere — a plain Win32 app
does not get COM initialized for it automatically. This was the actual
cause behind both prior rounds' dialog; the Runtime-presence theory
(round 1) and the detection bug (round 2) were both real, separate
issues, just not this one.

**Fix** (pushed): `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)`
near the top of `wWinMain`, paired with `CoUninitialize()` on every exit
path, plus linking `Ole32` in `windows/launcher/CMakeLists.txt`
(`Oleaut32` was already linked for `_com_error::ErrorMessage()`, a
separate library with a similar name — `Ole32` is the one that actually
provides `CoInitializeEx`/`CoUninitialize`).

## How the evidence in this document was produced

**Status: VERIFIED (2026-11-02) — the standalone launch genuinely
succeeds**, confirmed by run
[41383120271](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41383120271)'s
own text log (`gh run view 41383120271 --log`), read directly rather
than estimated or assumed:

- `100% tests passed out of 63` (ctest, Release, Windows).
- `Launch.exe still running after 6s: True`.
- `All top-level window titles: NEXUS Engineering Intelligence
  Workbench | ...` — the real application window title, present and
  correctly rendered (no mojibake); no "Startup Error" window anywhere
  in that list.
- `Startup error dialog detected (substring match): False` — the exact
  check that correctly caught the real failure in round 2's run now
  correctly reports none.
- `No launcher_error.log was written (no WebView2 init failure
  occurred, or Launch.exe could not write to its own directory).` —
  the file `LogStartupFailure()` writes on any WebView2 init failure
  was not produced at all, consistent with a clean startup.

**One honest caveat:** this project's sandboxed environment still
cannot download the run's binary artifacts (`windows_launch_screenshot.png`,
`NEXUS_Windows_x64.zip` — the Azure Blob Storage host they're served
from is blocked by this environment's network egress policy, the same
limit noted throughout this document and `AI_USE_LOG.md`), so the
screenshot's pixels have not been personally looked at by this
project directly this time either. What is different from the two
prior "VERIFIED" claims that turned out wrong is that this run's own
*text* log — which is exactly the data the launch-verification step
inspects to decide pass/fail, and which caught both prior real
failures correctly — independently confirms the same thing a visual
inspection would: the real app window, no error dialog, no error log.
Pulling `windows_launch_screenshot.png` from that run's Artifacts
section for a final visual confirmation is still worthwhile before
treating this as closed, exactly as it was what caught the original
defect.

- Repository:
  [github.com/jagaspeakstou-netizen/nexus-workbench](https://github.com/jagaspeakstou-netizen/nexus-workbench)
- **Run #1** (2026-11-01,
  [run 41338201277](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41338201277)) —
  **failed**, and the failure itself is genuine, disclosed evidence, not
  hidden. vcpkg manifest mode correctly detected the MSVC compiler and
  built GoogleTest 1.18.0 from source in 1.2 minutes (real, unedited log
  output). CMake configure then failed: `could not find any instance of
  Visual Studio` against the hardcoded generator `"Visual Studio 17
  2022"` — the same log's own compiler-detection step had just reported
  the installed toolset under `.../Visual Studio/18/Enterprise/...`,
  meaning that runner image's Visual Studio major version had moved past
  what the generator string named. Fixed by removing the hardcoded `-G`
  and letting CMake auto-select the installed generator (`-A x64` kept
  explicit, since a VS generator's default platform is Win32).
- **Run #2** (2026-11-01,
  [run 41338521566](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41338521566)) —
  **passed, every step**, after that one-line fix. What that run's own
  log actually shows:
  - CMake configured against **MSVC 19.51.36260.0** (Visual Studio 18
    Enterprise), vcpkg manifest mode installed `gtest:x64-windows@1.18.0`.
  - `cmake --build` produced `Launch.exe` (159,232 bytes) and
    `nexus_server.exe` (420,352 bytes), with the built `frontend/`
    directory copied alongside — exactly the relocatable folder layout
    this document describes below. The CMake configure log shows
    `Using statically-linked WebView2 loader` (the
    `WebView2LoaderStatic.lib` path in `windows/launcher/CMakeLists.txt`
    was taken, so there's no `WebView2Loader.dll` to bundle on this SDK
    version — confirmed by the build output directory listing containing
    only `Launch.exe`, `nexus_server.exe`, and `frontend/`).
  - `ctest` on that Windows build: **43/43 tests passed (100%)**, 0.71s
    total — the same `nexus_tests` suite that runs on Linux, now run and
    passing on real Windows.
  - The runner then ran `Start-Process Launch.exe`, waited 6 seconds, and
    checked `-not $proc.HasExited` — the workflow step is written to
    `throw` (failing the job) if the process had already exited by then;
    it did not, so **`Launch.exe` was still running 6 seconds after
    launch on a clean GitHub-hosted Windows 11 runner**. It then took a
    full-screen screenshot, confirmed the process was still alive a
    second time, and force-stopped it (clean shutdown, not a crash).
  - Artifacts uploaded by that run (`nexus-windows-evidence`):
    `windows_launch_screenshot.png`, `launch_status.txt` (the raw
    `Get-Process` output at screenshot time), and `NEXUS_Windows_x64.zip`
    (the exact packaged folder — downloadable and independently testable
    on any Windows 11 machine from the run's
    [Artifacts section](https://github.com/jagaspeakstou-netizen/nexus-workbench/actions/runs/41338521566)).

**One disclosed gap:** this project's own sandboxed environment could not
download the artifact `.zip`/`.png` binaries itself (its network egress
policy blocks the Azure Blob Storage host GitHub's artifact download uses)
— only the run's text logs, which is everything quoted above. The
screenshot's *existence* and the launch step's pass/fail gate are
independently verified from the log; its visual *contents* have not been
looked at by this project directly and should be pulled from the
Artifacts link above before relying on it as interview evidence.

This document, the launcher source
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
