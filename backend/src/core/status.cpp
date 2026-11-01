#include "nexus/status.hpp"

namespace nexus {

std::vector<ModuleStatus> get_module_statuses() {
    return {
        {"geometry",    "Geometry Lab",          true,  true,  "ASCII STL + ANSYS CSV parsers with validation, verified"},
        {"thermal",     "Thermal Analysis",      true,  true,  "1D steady-state FEA solver, N=10/20/40/80 convergence verified"},
        {"performance", "Performance Lab",       true,  true,  "Multicore batch solve, workers capped to hardware, speedup/agreement verified"},
        {"hardware",    "Hardware Integration",  false, false, "Pending - Day 10"},
        {"neural",      "Neural Computation",    false, false, "Pending - Day 9"},
        {"validation",  "Validation & Tests",    false, false, "Pending - Day 11"},
        {"docs",        "Documentation",         false, false, "Pending - Day 14"},
    };
}

BuildInfo get_build_info() {
    return BuildInfo{"NEXUS Engineering Intelligence Workbench", "0.1.0-skeleton", "C++17"};
}

}  // namespace nexus
