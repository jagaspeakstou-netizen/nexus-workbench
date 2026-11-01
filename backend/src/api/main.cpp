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
#include <fstream>
#include <iostream>
#include <sstream>

#include "nexus/geometry.hpp"
#include "nexus/performance.hpp"
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

json stl_result_to_json(const nexus::geometry::StlParseResult& r) {
    if (!r.valid) {
        return json{{"valid", false}, {"error", r.error}};
    }
    json triangles = json::array();
    for (const auto& t : r.triangles) {
        triangles.push_back({
            {"normal", {t.normal.x, t.normal.y, t.normal.z}},
            {"v0", {t.v0.x, t.v0.y, t.v0.z}},
            {"v1", {t.v1.x, t.v1.y, t.v1.z}},
            {"v2", {t.v2.x, t.v2.y, t.v2.z}},
        });
    }
    return json{
        {"valid", true},
        {"solid_name", r.solid_name},
        {"triangle_count", r.triangles.size()},
        {"bounding_box", {
            {"min", {r.bbox.min.x, r.bbox.min.y, r.bbox.min.z}},
            {"max", {r.bbox.max.x, r.bbox.max.y, r.bbox.max.z}},
        }},
        {"triangles", triangles},
    };
}

json csv_result_to_json(const nexus::geometry::CsvParseResult& r) {
    if (!r.valid) {
        return json{{"valid", false}, {"error", r.error}};
    }
    json nodes = json::array();
    for (const auto& n : r.nodes) {
        nodes.push_back({
            {"node_id", n.node_id},
            {"x_m", n.x},
            {"y_m", n.y},
            {"z_m", n.z},
            {"temperature_C", n.temperature},
        });
    }
    return json{
        {"valid", true},
        {"node_count", r.nodes.size()},
        {"min_temperature_C", r.min_temperature},
        {"max_temperature_C", r.max_temperature},
        {"avg_temperature_C", r.avg_temperature},
        {"nodes", nodes},
    };
}

json performance_report_to_json(const nexus::performance::BenchmarkReport& r) {
    json entries = json::array();
    for (const auto& e : r.entries) {
        entries.push_back({
            {"num_workers", e.num_workers},
            {"rep_times_seconds", e.rep_times_seconds},
            {"median_seconds", e.median_seconds},
            {"speedup", e.speedup},
            {"efficiency", e.efficiency},
        });
    }
    return json{
        {"config", {
            {"seed", r.config.seed},
            {"case_count", r.config.case_count},
            {"mesh_elements", r.config.mesh_elements},
            {"k_min", r.config.k_min}, {"k_max", r.config.k_max},
            {"q_min", r.config.q_min}, {"q_max", r.config.q_max},
            {"repetitions", r.config.repetitions},
        }},
        {"hardware", {
            {"cpu_model", r.hardware.cpu_model},
            {"logical_cores", r.hardware.logical_cores},
            {"physical_cores", r.hardware.physical_cores},
            {"physical_cores_is_estimate", r.hardware.physical_cores_is_estimate},
            {"ram_mb", r.hardware.ram_mb},
            {"compiler", r.hardware.compiler},
            {"build_type", r.hardware.build_type},
            {"optimization_flags", r.hardware.optimization_flags},
        }},
        {"entries", entries},
        {"max_abs_agreement_error_C", r.max_abs_agreement_error_C},
        {"warmup_note", r.warmup_note},
        {"launch_to_ready_note", r.launch_to_ready_note},
    };
}

// Sample data files are looked up relative to a few likely working
// directories so the server behaves the same whether it's launched from
// backend/build (typical local dev) or the repo root (e.g. from a
// packaged launcher). Returns std::nullopt if none of the candidates
// exist -- callers turn that into a clear 404, not a crash.
std::optional<std::string> read_sample_file(const std::string& relative_name) {
    const std::vector<std::string> candidates = {
        "../../data/geometry/" + relative_name,
        "../data/geometry/" + relative_name,
        "data/geometry/" + relative_name,
    };
    for (const auto& path : candidates) {
        std::ifstream in(path, std::ios::binary);
        if (in) {
            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }
    }
    return std::nullopt;
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

    server.Post("/api/geometry/stl/parse", [](const httplib::Request& req, httplib::Response& res) {
        const auto result = nexus::geometry::parse_ascii_stl(req.body);
        res.set_content(stl_result_to_json(result).dump(), "application/json");
    });

    server.Post("/api/geometry/csv/parse", [](const httplib::Request& req, httplib::Response& res) {
        const auto result = nexus::geometry::parse_ansys_csv(req.body);
        res.set_content(csv_result_to_json(result).dump(), "application/json");
    });

    server.Get("/api/geometry/sample/stl", [](const httplib::Request&, httplib::Response& res) {
        const auto content = read_sample_file("sample_tetrahedron.stl");
        if (!content) {
            res.status = 404;
            res.set_content(json{{"error", "Sample STL file not found on server."}}.dump(), "application/json");
            return;
        }
        res.set_content(*content, "text/plain");
    });

    server.Get("/api/geometry/sample/csv", [](const httplib::Request&, httplib::Response& res) {
        const auto content = read_sample_file("ansys_nodal_temperatures_synthetic.csv");
        if (!content) {
            res.status = 404;
            res.set_content(json{{"error", "Sample CSV file not found on server."}}.dump(), "application/json");
            return;
        }
        res.set_content(*content, "text/plain");
    });

    server.Get("/api/performance/benchmark", [](const httplib::Request&, httplib::Response& res) {
        // A lighter configuration than the CLI's default (see
        // nexus_performance_export) so a live HTTP request stays fast;
        // same solver, same validation, just fewer cases/repetitions.
        nexus::performance::BenchmarkConfig config;
        config.case_count = 1200;
        config.mesh_elements = 100;
        config.repetitions = 5;
        config.worker_counts = {1, 2, 3, 4};

        const auto report = nexus::performance::run_full_benchmark(config);
        res.set_content(performance_report_to_json(report).dump(), "application/json");
    });

    std::cout << "NEXUS backend listening on http://127.0.0.1:" << port << "\n";
    std::cout << "Try: curl http://127.0.0.1:" << port << "/api/status\n";

    if (!server.listen("127.0.0.1", port)) {
        std::cerr << "Failed to bind to port " << port << "\n";
        return 1;
    }
    return 0;
}
