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

    std::cout << "NEXUS backend listening on http://127.0.0.1:" << port << "\n";
    std::cout << "Try: curl http://127.0.0.1:" << port << "/api/status\n";

    if (!server.listen("127.0.0.1", port)) {
        std::cerr << "Failed to bind to port " << port << "\n";
        return 1;
    }
    return 0;
}
