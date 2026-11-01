// NEXUS backend entry point.
//
// Phase 1 (walking skeleton): exposes a single read-only endpoint,
// GET /api/status, so the frontend has something real to fetch while
// later phases fill in the actual computational modules behind their
// own endpoints (geometry, thermal, performance, hardware, neural).
//
// Keeping this file thin is deliberate (Task A: "clear separation
// between C++ core and presentation/transport code") -- all real logic
// lives in nexus_core (see include/nexus/*.hpp, src/core/*.cpp); this
// file only wires HTTP routes to it.

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

#include "nexus/status.hpp"
#include "nexus/thermal.hpp"

using json = nlohmann::json;

namespace {

json module_status_to_json(const nexus::ModuleStatus& m) {
    return json{
        {"id", m.id},
        {"label", m.label},
        {"implemented", m.implemented},
        {"tested", m.tested},
        {"note", m.note},
    };
}

json thermal_result_to_json(const nexus::thermal::SolveResult& r) {
    json nodes = json::array();
    for (const auto& n : r.nodes) {
        nodes.push_back({{"index", n.index}, {"x_m", n.x}, {"temperature_C", n.temperature}});
    }
    json midpoints = json::array();
    for (const auto& m : r.midpoints) {
        midpoints.push_back({
            {"element_index", m.element_index},
            {"x_m", m.x},
            {"fe_temperature_C", m.fe_temperature},
            {"exact_temperature_C", m.exact_temperature},
            {"abs_error_C", m.abs_error},
        });
    }
    return json{
        {"num_elements", r.num_elements},
        {"element_size_h_m", r.element_size_h},
        {"nodes", nodes},
        {"midpoints", midpoints},
        {"max_abs_midpoint_error_C", r.max_abs_midpoint_error},
        {"min_temperature_C", r.min_temperature},
        {"max_temperature_C", r.max_temperature},
    };
}

}  // namespace

int main(int argc, char** argv) {
    int port = 8787;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port argument, using default " << port << "\n";
        }
    }

    httplib::Server server;

    // CORS: the Vite dev server runs on a different port during development.
    server.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
    });

    server.Get("/api/status", [](const httplib::Request&, httplib::Response& res) {
        const auto build = nexus::get_build_info();
        const auto modules = nexus::get_module_statuses();

        json modules_json = json::array();
        for (const auto& m : modules) {
            modules_json.push_back(module_status_to_json(m));
        }

        json body{
            {"app_name", build.app_name},
            {"version", build.version},
            {"cpp_standard", build.cpp_standard},
            {"modules", modules_json},
        };

        res.set_content(body.dump(), "application/json");
    });

    server.Get("/api/thermal/study", [](const httplib::Request&, httplib::Response& res) {
        nexus::thermal::ProblemDefinition problem;
        const std::vector<int> mesh_sizes{10, 20, 40, 80};
        const auto results = nexus::thermal::mesh_convergence_study(problem, mesh_sizes);

        json runs = json::array();
        for (const auto& r : results) {
            runs.push_back(thermal_result_to_json(r));
        }

        json convergence_n = json::array();
        json convergence_h = json::array();
        json convergence_err = json::array();
        for (const auto& r : results) {
            convergence_n.push_back(r.num_elements);
            convergence_h.push_back(r.element_size_h);
            convergence_err.push_back(r.max_abs_midpoint_error);
        }

        json body{
            {"problem", {
                {"L_m", problem.L},
                {"A_m2", problem.A},
                {"k_W_per_mK", problem.k},
                {"q_W_per_m3", problem.q},
                {"T0_C", problem.T0},
                {"TL_C", problem.TL},
            }},
            {"runs", runs},
            {"convergence", {
                {"num_elements", convergence_n},
                {"element_size_h_m", convergence_h},
                {"max_abs_midpoint_error_C", convergence_err},
            }},
        };
        res.set_content(body.dump(), "application/json");
    });

    std::cout << "NEXUS backend listening on http://127.0.0.1:" << port << "\n";
    std::cout << "Try: curl http://127.0.0.1:" << port << "/api/status\n";

    if (!server.listen("127.0.0.1", port)) {
        std::cerr << "Failed to bind to port " << port << "\n";
        return 1;
    }
    return 0;
}
