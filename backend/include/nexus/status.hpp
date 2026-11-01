#pragma once

#include <string>
#include <vector>

namespace nexus {

// One entry in the Overview dashboard's module list.
// `implemented` / `tested` are honest state flags -- the Overview page
// renders them directly, so this struct is the single source of truth
// for "what actually works right now" shown to the evaluator.
struct ModuleStatus {
    std::string id;           // e.g. "thermal"
    std::string label;        // e.g. "Thermal Analysis"
    bool implemented;
    bool tested;
    std::string note;         // short human-readable status note
};

// Returns the current status of every module in the workbench.
// Phase 1 (this file): only the API/core module itself is "implemented";
// every other module is honestly reported as pending until its own
// phase lands. This function is the thing Task A's "no decorative
// controls" rule is checked against -- it must reflect reality.
std::vector<ModuleStatus> get_module_statuses();

// Simple build/version info returned by /api/status for sanity checks.
struct BuildInfo {
    std::string app_name;
    std::string version;
    std::string cpp_standard;
};

BuildInfo get_build_info();

}  // namespace nexus
