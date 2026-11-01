#include "nexus/status.hpp"

namespace nexus {

std::vector<ModuleStatus> get_module_statuses() {
    return {
        {"geometry",    "Geometry Lab",          false, false, "Pending - Day 6-7"},
        {"thermal",     "Thermal Analysis",      false, false, "Pending - Day 3-4"},
        {"performance", "Performance Lab",       false, false, "Pending - Day 8"},
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
