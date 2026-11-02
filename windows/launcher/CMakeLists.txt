# Windows launcher shell (Launch.exe) — only configured when the parent
# project is built with WIN32 (see the `if(WIN32)` guard in
# backend/CMakeLists.txt, which add_subdirectory()s this folder). This
# target is NOT built on the Linux/macOS dev container this project was
# developed on — it requires the Windows SDK and the WebView2 SDK.
#
# WHY A SEPARATE CMakeLists.txt: keeping the Windows-only target in its own
# subdirectory, rather than inline in backend/CMakeLists.txt, means the
# WebView2 NuGet restore (network access, Windows-only tool) only ever runs
# when actually configuring on Windows — the cross-platform backend build
# used for day-to-day development stays untouched.

cmake_minimum_required(VERSION 3.16)

# Statically link the MSVC C/C++ runtime so Launch.exe does not require the
# Visual C++ Redistributable to be installed on the target machine — a
# clean Windows 11 install with no dev tools can run it as-is.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_program(NUGET_EXE nuget)
if(NOT NUGET_EXE)
  message(FATAL_ERROR
    "nuget.exe not found on PATH. The windows-latest GitHub Actions runner "
    "ships it as part of the Visual Studio toolchain; on a local Windows "
    "machine, run this from a 'Developer Command Prompt for VS' or install "
    "the NuGet CLI (https://www.nuget.org/downloads).")
endif()

set(WEBVIEW2_PKG_DIR "${CMAKE_BINARY_DIR}/webview2_packages")
execute_process(
  COMMAND "${NUGET_EXE}" install Microsoft.Web.WebView2 -OutputDirectory "${WEBVIEW2_PKG_DIR}" -ExcludeVersion
  RESULT_VARIABLE NUGET_RESULT
  OUTPUT_VARIABLE NUGET_OUTPUT
  ERROR_VARIABLE NUGET_ERROR
)
if(NOT NUGET_RESULT EQUAL 0)
  message(FATAL_ERROR
    "Failed to restore the Microsoft.Web.WebView2 NuGet package "
    "(exit ${NUGET_RESULT}).\nstdout: ${NUGET_OUTPUT}\nstderr: ${NUGET_ERROR}")
endif()
message(STATUS "WebView2 SDK restored to ${WEBVIEW2_PKG_DIR}")

set(WEBVIEW2_DIR "${WEBVIEW2_PKG_DIR}/Microsoft.Web.WebView2")

add_executable(nexus_launcher WIN32 Launcher.cpp)
target_compile_definitions(nexus_launcher PRIVATE UNICODE _UNICODE)
target_include_directories(nexus_launcher PRIVATE "${WEBVIEW2_DIR}/build/native/include")
set_target_properties(nexus_launcher PROPERTIES OUTPUT_NAME "Launch")

# Defense in depth: a 2026-11-02 run showed MSVC mis-decoding a literal
# non-ASCII character embedded in this project's UTF-8-saved .cpp files
# (an em-dash rendered as mojibake in a message-box title) because the
# compiler's source charset wasn't explicitly set. Launcher.cpp's string
# literals have since been rewritten to use \u escapes instead (immune to
# this regardless), but this flag is added too so any future source file
# in this target that does embed a literal non-ASCII character is decoded
# correctly rather than silently mangled at compile time.
if(MSVC)
  target_compile_options(nexus_launcher PRIVATE /utf-8)
endif()

# Prefer the statically-linked loader (no extra DLL to ship); fall back to
# the dynamic loader + its DLL if a given SDK version doesn't ship the
# static variant. Whichever path is taken is recorded in the CMake
# configure log, which is captured as part of this project's CI evidence.
set(WEBVIEW2_STATIC_LIB "${WEBVIEW2_DIR}/build/native/x64/WebView2LoaderStatic.lib")
set(WEBVIEW2_DYNAMIC_LIB "${WEBVIEW2_DIR}/build/native/x64/WebView2Loader.lib")
set(WEBVIEW2_DYNAMIC_DLL "${WEBVIEW2_DIR}/build/native/x64/WebView2Loader.dll")

if(EXISTS "${WEBVIEW2_STATIC_LIB}")
  message(STATUS "Using statically-linked WebView2 loader: ${WEBVIEW2_STATIC_LIB}")
  target_link_libraries(nexus_launcher PRIVATE "${WEBVIEW2_STATIC_LIB}")
elseif(EXISTS "${WEBVIEW2_DYNAMIC_LIB}")
  message(STATUS "Static WebView2 loader not found in this SDK version; "
                  "using dynamic loader + bundling WebView2Loader.dll")
  target_link_libraries(nexus_launcher PRIVATE "${WEBVIEW2_DYNAMIC_LIB}")
  add_custom_command(TARGET nexus_launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${WEBVIEW2_DYNAMIC_DLL}"
      "$<TARGET_FILE_DIR:nexus_launcher>/WebView2Loader.dll"
  )
else()
  message(FATAL_ERROR
    "Neither WebView2LoaderStatic.lib nor WebView2Loader.lib was found "
    "under ${WEBVIEW2_DIR}/build/native/x64/ - the restored SDK's layout "
    "did not match what this CMakeLists.txt expects. List that directory "
    "and adjust the paths above.")
endif()

# Oleaut32 is needed for _com_error::ErrorMessage() (comdef.h, used by
# LogStartupFailure in Launcher.cpp to turn a raw HRESULT into real
# diagnostic text); Ole32 for CoInitializeEx/CoUninitialize, added after a
# real CI run (2026-11-02) showed WebView2 init failing with
# CO_E_NOTINITIALIZED because this app never initialized COM -- the actual
# root cause of the startup-error dialog investigated across AI_USE_LOG.md
# Item 8. Neither is auto-linked by a bare CMake executable target the way
# a Visual Studio project template would link them.
target_link_libraries(nexus_launcher PRIVATE Shlwapi Oleaut32 Ole32)

# Stage the backend executable and the built frontend beside Launch.exe so
# the whole output directory is a single relocatable application folder.
add_dependencies(nexus_launcher nexus_server)

set(FRONTEND_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../frontend/dist")
add_custom_command(TARGET nexus_launcher POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:nexus_server>"
    "$<TARGET_FILE_DIR:nexus_launcher>/nexus_server.exe"
  COMMENT "Copying nexus_server.exe next to Launch.exe"
)

if(EXISTS "${FRONTEND_DIST_DIR}")
  add_custom_command(TARGET nexus_launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:nexus_launcher>/frontend"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${FRONTEND_DIST_DIR}"
      "$<TARGET_FILE_DIR:nexus_launcher>/frontend"
    COMMENT "Copying built frontend next to Launch.exe"
  )
else()
  message(WARNING
    "frontend/dist does not exist yet (${FRONTEND_DIST_DIR}). Run "
    "'npm ci && npm run build' in frontend/ BEFORE configuring/building "
    "this CMake project, or Launch.exe will have no UI to display. The CI "
    "workflow (.github/workflows/windows-build.yml) does this automatically.")
endif()
