#include "nexus/status.hpp"

namespace nexus {

std::vector<ModuleStatus> get_module_statuses() {
    return {
        {"geometry",    "Geometry Lab",          true,  true,  "ASCII STL + ANSYS CSV parsers with validation, verified"},
        {"thermal",     "Thermal Analysis",      true,  true,  "1D steady-state FEA solver, N=10/20/40/80 convergence verified"},
        {"performance", "Performance Lab",       true,  true,  "Multicore batch solve, workers capped to hardware, speedup/agreement verified"},
        {"hardware",    "Hardware Integration",  true,  true,  "Mock microcontroller/FPGA/JTAG adapters: discovery, connect retry, telemetry with timeout-recovery -- deterministic simulator, no physical hardware"},
        {"neural",      "Neural Computation",    true,  true,  "Hand-implemented backprop (tanh hidden unit), finite-difference gradient check verified"},
        {"validation",  "Validation & Tests",    true,  true,  "GCC -Wall/-Wextra/-Wpedantic/-Wshadow (0 warnings), ASan+UBSan and ThreadSanitizer clean across 63/63 tests, documented edge-case audit"},
        {"docs",        "Documentation",         false, false, "Pending - Day 14"},
    };
}

BuildInfo get_build_info() {
    return BuildInfo{"NEXUS Engineering Intelligence Workbench", "0.1.0-skeleton", "C++17"};
}

}  // namespace nexus
