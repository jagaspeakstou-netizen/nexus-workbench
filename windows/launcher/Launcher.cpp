// NEXUS Engineering Intelligence Workbench — Windows launcher shell.
//
// This is NOT the application itself. It is a thin native Win32 host that:
//   1. Spawns the already-built nexus_server.exe (the real C++ backend,
//      same binary produced by backend/CMakeLists.txt) as a child process.
//   2. Hosts a Microsoft Edge WebView2 control and navigates it to the
//      already-built frontend (frontend/dist/index.html), which talks to
//      the backend over http://127.0.0.1:8787 exactly as it does in dev.
//   3. Terminates the backend child process when the window closes.
//
// Deliberate simplifications (disclosed, not hidden):
//   - The readiness wait before first navigation is a fixed Sleep(), not a
//     socket-poll loop. The frontend's own pages already render an honest
//     "Backend unreachable" error state with a retry control (see
//     frontend/src/services/api.ts's ApiError and each page's useEffect),
//     so a user who opens the window before the backend has bound its port
//     sees a correct error, not a crash or a blank screen, and can retry.
//   - No installer, no admin elevation, no registry writes. The whole
//     application folder (Launch.exe, nexus_server.exe, frontend/, and
//     WebView2Loader.dll if the static loader lib wasn't available at
//     build time — see windows/launcher/CMakeLists.txt) is relocatable;
//     Launch.exe locates its own siblings via GetModuleFileNameW, never a
//     hardcoded path.
//
// Requires the Microsoft Edge WebView2 Runtime, which Windows 11 ships
// pre-installed by default (the "Evergreen" runtime, serviced by Windows
// Update — not bundled in this folder). See docs/packaging/windows.md for
// the fallback behavior on a machine where it's missing, and for the
// actual clean-machine verification evidence this project collected.

#include <windows.h>
#include <objbase.h>  // CoInitializeEx / CoUninitialize
#include <shlwapi.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <fstream>
#include <comdef.h>
#include "WebView2.h"

#pragma comment(lib, "shlwapi.lib")

using Microsoft::WRL::Callback;

namespace {

constexpr wchar_t kWindowClassName[] = L"NexusLauncherWindow";
constexpr wchar_t kWindowTitle[] = L"NEXUS Engineering Intelligence Workbench";

Microsoft::WRL::ComPtr<ICoreWebView2Controller> g_webviewController;
Microsoft::WRL::ComPtr<ICoreWebView2> g_webview;
HANDLE g_backendProcess = nullptr;

// Directory containing Launch.exe itself, with a trailing backslash.
// Everything this app needs (nexus_server.exe, frontend/) lives beside it,
// so the whole folder can be moved, zipped, or run from any drive letter.
std::wstring GetAppDirectory() {
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  PathRemoveFileSpecW(path);
  std::wstring dir(path);
  if (!dir.empty() && dir.back() != L'\\') {
    dir += L'\\';
  }
  return dir;
}

// Spawns nexus_server.exe (the real backend) as a child process with no
// visible console window. Returns its process handle, or nullptr on
// failure (the WebView2 error path then renders instead of a crash).
HANDLE StartBackend(const std::wstring& appDir) {
  std::wstring exePath = appDir + L"nexus_server.exe";

  STARTUPINFOW startupInfo = {};
  startupInfo.cb = sizeof(startupInfo);
  PROCESS_INFORMATION processInfo = {};

  // CreateProcessW may write into the command-line buffer; it must not be
  // a read-only string literal.
  std::wstring commandLine = L"\"" + exePath + L"\"";
  std::vector<wchar_t> commandLineBuffer(commandLine.begin(), commandLine.end());
  commandLineBuffer.push_back(L'\0');

  BOOL ok = CreateProcessW(
      exePath.c_str(),
      commandLineBuffer.data(),
      nullptr, nullptr,
      /*bInheritHandles=*/FALSE,
      CREATE_NO_WINDOW,
      nullptr,
      appDir.c_str(),
      &startupInfo,
      &processInfo);

  if (!ok) {
    return nullptr;
  }
  CloseHandle(processInfo.hThread);
  return processInfo.hProcess;
}

void StopBackend() {
  if (g_backendProcess != nullptr) {
    TerminateProcess(g_backendProcess, 0);
    CloseHandle(g_backendProcess);
    g_backendProcess = nullptr;
  }
}

void ResizeWebViewToClientArea(HWND hwnd) {
  if (g_webviewController == nullptr) {
    return;
  }
  RECT bounds;
  GetClientRect(hwnd, &bounds);
  g_webviewController->put_Bounds(bounds);
}

// Title uses an explicit — escape (not a literal em-dash byte sequence
// in the source file) deliberately: a real run on 2026-11-02 showed this
// title rendered as mojibake ("NEXUS \xE2\x82\xAC\x94 Startup Error") because
// MSVC, without an explicit UTF-8 source-charset flag, mis-decoded the UTF-8
// bytes of a literal em-dash in this .cpp file as three separate CP1252
// characters. An escape sequence is ASCII in the source and immune to that
// regardless of the compiler's assumed source encoding -- the actual bug
// found on 2026-11-02, see docs/packaging/windows.md and AI_USE_LOG.md Item 8.
constexpr wchar_t kStartupErrorTitle[] = L"NEXUS \u2014 Startup Error";

// Appends a line to launcher_error.log beside Launch.exe, so a startup
// failure leaves a real, inspectable HRESULT behind instead of only a
// generic message box -- added after a 2026-11-02 CI run showed the
// WebView2 Runtime WAS present (v155.0.4283.62) yet initialization still
// failed, meaning "runtime missing" was the wrong diagnosis and nothing
// in this file was capturing the actual error code to find the right one.
void LogStartupFailure(const std::wstring& appDir, const wchar_t* stage, HRESULT hr) {
  std::wofstream log(appDir + L"launcher_error.log", std::ios::app);
  if (!log) return;
  _com_error err(hr);
  log << L"[" << stage << L"] HRESULT=0x" << std::hex << static_cast<unsigned long>(hr)
      << L" (" << (err.ErrorMessage() ? err.ErrorMessage() : L"no message") << L")\n";
}

// Shows a plain Win32 message box describing a WebView2 setup failure, and
// logs the real HRESULT via LogStartupFailure (called by the caller before
// this) rather than only guessing "runtime missing" in the user-facing text.
void ShowWebView2MissingError() {
  MessageBoxW(
      nullptr,
      L"NEXUS could not start the Microsoft Edge WebView2 component.\n\n"
      L"If the WebView2 Runtime is missing, install it from "
      L"https://go.microsoft.com/fwlink/p/?LinkId=2124703 and relaunch NEXUS.\n\n"
      L"A diagnostic log was written to launcher_error.log beside this "
      L"application for troubleshooting if the problem persists.",
      kStartupErrorTitle,
      MB_OK | MB_ICONERROR);
}

void InitializeWebView(HWND hwnd, const std::wstring& appDir) {
  std::wstring userDataFolder = appDir + L"webview2_user_data";

  HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr,
      userDataFolder.c_str(),
      nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [hwnd, appDir](HRESULT envResult, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(envResult) || env == nullptr) {
              LogStartupFailure(appDir, L"CreateCoreWebView2EnvironmentWithOptions", envResult);
              ShowWebView2MissingError();
              return envResult;
            }

            env->CreateCoreWebView2Controller(
                hwnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [hwnd, appDir](HRESULT ctrlResult, ICoreWebView2Controller* controller) -> HRESULT {
                      if (FAILED(ctrlResult) || controller == nullptr) {
                        LogStartupFailure(appDir, L"CreateCoreWebView2Controller", ctrlResult);
                        ShowWebView2MissingError();
                        return ctrlResult;
                      }

                      g_webviewController = controller;
                      g_webviewController->get_CoreWebView2(&g_webview);

                      ResizeWebViewToClientArea(hwnd);

                      // The built SPA (frontend/dist, copied to frontend/
                      // beside Launch.exe by the CMake post-build step).
                      std::wstring indexPath = appDir + L"frontend\\index.html";
                      std::wstring indexUrl = L"file:///" + indexPath;
                      for (auto& c : indexUrl) {
                        if (c == L'\\') c = L'/';
                      }
                      g_webview->Navigate(indexUrl.c_str());
                      return S_OK;
                    })
                    .Get());
            return S_OK;
          })
          .Get());

  if (FAILED(hr)) {
    LogStartupFailure(appDir, L"CreateCoreWebView2EnvironmentWithOptions (synchronous)", hr);
    ShowWebView2MissingError();
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_SIZE:
      ResizeWebViewToClientArea(hwnd);
      return 0;
    case WM_DESTROY:
      StopBackend();
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProc(hwnd, message, wParam, lParam);
  }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  // Found via launcher_error.log on a real CI run (2026-11-02): without
  // this call, CreateCoreWebView2EnvironmentWithOptions fails with
  // HRESULT 0x800401f0 (CO_E_NOTINITIALIZED, "CoInitialize has not been
  // called") -- WebView2's environment creation is COM-based and this app
  // never initialized COM on its main thread. This was the actual root
  // cause of the WebView2 startup-error dialog seen in two prior CI runs;
  // neither the WebView2 Runtime's presence/absence nor the Windows
  // Server-vs-11-client distinction (both investigated first, see
  // AI_USE_LOG.md Item 8) was the real issue. COINIT_APARTMENTTHREADED
  // matches how a UI-owning thread hosting a COM control like WebView2 is
  // meant to initialize COM (STA).
  HRESULT coInitResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(coInitResult)) {
    MessageBoxW(nullptr,
                L"NEXUS failed to initialize COM (CoInitializeEx). This is "
                L"required before the WebView2 component can start.",
                kStartupErrorTitle, MB_OK | MB_ICONERROR);
    return 1;
  }

  // Per-monitor DPI awareness so the WebView2 control renders crisply on
  // high-DPI displays. Available on Windows 10 1703+; always present on
  // Windows 11.
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  const std::wstring appDir = GetAppDirectory();
  g_backendProcess = StartBackend(appDir);

  // Deliberate fixed wait, see the file-header note on why this isn't a
  // socket-poll loop.
  Sleep(400);

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = kWindowClassName;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  HWND hwnd = CreateWindowExW(
      0, kWindowClassName, kWindowTitle,
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
      nullptr, nullptr, hInstance, nullptr);

  if (hwnd == nullptr) {
    StopBackend();
    CoUninitialize();
    return 1;
  }

  InitializeWebView(hwnd, appDir);

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  StopBackend();
  CoUninitialize();
  return static_cast<int>(msg.wParam);
}
