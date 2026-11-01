#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nexus::geometry {

// ---------------------------------------------------------------------
// ASCII STL
// ---------------------------------------------------------------------

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

struct Triangle {
    Vec3 normal;
    Vec3 v0, v1, v2;
};

struct BoundingBox {
    Vec3 min;
    Vec3 max;
};

struct StlParseResult {
    bool valid = false;
    std::string error;          // populated iff !valid; includes a line number where known
    std::string solid_name;
    std::vector<Triangle> triangles;
    BoundingBox bbox{};
};

// Parses an ASCII STL file's text content. Strictly validates the
// "solid ... facet normal ... outer loop ... vertex x3 ... endloop
// endfacet ... endsolid" grammar; rejects anything that doesn't match
// it (including binary STL, which will fail to tokenize as this
// grammar), rejects a solid with zero triangles (an "empty" STL), and
// rejects any non-finite (NaN/Inf) coordinate or normal component.
// `max_bytes` bounds input size (default 5 MiB) -- oversized input is
// rejected before parsing, not truncated.
StlParseResult parse_ascii_stl(const std::string& content, std::size_t max_bytes = 5 * 1024 * 1024);

// ---------------------------------------------------------------------
// ANSYS nodal temperature CSV
// ---------------------------------------------------------------------

struct AnsysNode {
    long node_id = 0;
    double x = 0.0, y = 0.0, z = 0.0;  // meters
    double temperature = 0.0;          // degrees C
};

struct CsvParseResult {
    bool valid = false;
    std::string error;  // populated iff !valid; includes a row number where known
    std::vector<AnsysNode> nodes;
    double min_temperature = 0.0;
    double max_temperature = 0.0;
    double avg_temperature = 0.0;
};

// Parses an ANSYS-style nodal temperature export: a CSV with header
// exactly "node_id,x_m,y_m,z_m,temperature_C" followed by one row per
// node. Rejects a missing/renamed/reordered header, rows with the
// wrong field count, non-numeric or non-finite fields, and an
// otherwise-valid header with zero data rows (an "empty" file).
CsvParseResult parse_ansys_csv(const std::string& content, std::size_t max_bytes = 5 * 1024 * 1024);

}  // namespace nexus::geometry
