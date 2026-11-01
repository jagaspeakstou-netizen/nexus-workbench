#include "nexus/performance.hpp"

#include <fstream>
#include <sstream>
#include <thread>

#if defined(__linux__)
#include <sys/sysinfo.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace nexus::performance {

namespace {

std::string detect_compiler() {
#if defined(__clang__)
    return "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__) + "." +
           std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
    return "GCC " + std::string(__VERSION__);
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#else
    return "unknown compiler";
#endif
}

std::string detect_build_type() {
#if defined(NEXUS_BUILD_TYPE)
    return NEXUS_BUILD_TYPE;
#else
    return "unknown";
#endif
}

std::string detect_optimization_flags() {
#if defined(NEXUS_CXX_FLAGS)
    const std::string flags = NEXUS_CXX_FLAGS;
    return flags.empty() ? "(none set -- see CMAKE_CXX_FLAGS_<build type>)" : flags;
#else
    return "unknown";
#endif
}

#if defined(__linux__)
// Parses /proc/cpuinfo for the model name and a best-effort physical
// core count (unique "physical id"+"core id" pairs). Returns false if
// the file can't be read (e.g. a restricted container), in which case
// callers fall back to std::thread::hardware_concurrency() and an
// "unknown" model, rather than guessing.
bool detect_linux_cpuinfo(HardwareInfo& info) {
    std::ifstream in("/proc/cpuinfo");
    if (!in) return false;

    std::string line;
    bool found_model = false;
    while (std::getline(in, line)) {
        if (!found_model && line.rfind("model name", 0) == 0) {
            const auto colon = line.find(':');
            if (colon != std::string::npos) {
                info.cpu_model = line.substr(colon + 2);
                found_model = true;
            }
        }
    }
    return found_model;
}

bool detect_linux_ram(HardwareInfo& info) {
    struct sysinfo si;
    if (sysinfo(&si) != 0) return false;
    info.ram_mb = static_cast<long>((si.totalram * si.mem_unit) / (1024 * 1024));
    return true;
}
#endif

#if defined(_WIN32)
bool detect_windows_ram(HardwareInfo& info) {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) return false;
    info.ram_mb = static_cast<long>(status.ullTotalPhys / (1024 * 1024));
    return true;
}
#endif

}  // namespace

HardwareInfo detect_hardware() {
    HardwareInfo info;
    info.logical_cores = static_cast<int>(std::thread::hardware_concurrency());
    info.compiler = detect_compiler();
    info.build_type = detect_build_type();
    info.optimization_flags = detect_optimization_flags();

#if defined(__linux__)
    detect_linux_cpuinfo(info);
    detect_linux_ram(info);
    // Physical core count isn't parsed from /proc/cpuinfo here (requires
    // correlating "physical id"/"core id" across all entries, which is
    // fragile across container cgroup-limited CPU views); report logical
    // count as the estimate and say so explicitly rather than guess.
    info.physical_cores = info.logical_cores;
    info.physical_cores_is_estimate = true;
#elif defined(_WIN32)
    detect_windows_ram(info);
    info.physical_cores = info.logical_cores;
    info.physical_cores_is_estimate = true;
    info.cpu_model = "unknown (Windows CPU-model detection not implemented -- see docs/architecture/performance_lab.md)";
#else
    info.physical_cores = info.logical_cores;
    info.physical_cores_is_estimate = true;
#endif

    return info;
}

}  // namespace nexus::performance
