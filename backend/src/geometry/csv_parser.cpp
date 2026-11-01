#include "nexus/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace nexus::geometry {

namespace {

const std::vector<std::string> kExpectedHeader = {"node_id", "x_m", "y_m", "z_m", "temperature_C"};

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::ostringstream cur;
    for (char c : line) {
        if (c == ',') {
            fields.push_back(cur.str());
            cur.str("");
        } else if (c != '\r') {
            cur << c;
        }
    }
    fields.push_back(cur.str());
    return fields;
}

std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

}  // namespace

CsvParseResult parse_ansys_csv(const std::string& content, std::size_t max_bytes) {
    CsvParseResult result;

    if (content.size() > max_bytes) {
        result.error = "File exceeds the " + std::to_string(max_bytes / (1024 * 1024)) +
                        " MiB size limit for CSV uploads.";
        return result;
    }
    if (content.find_first_not_of(" \t\r\n") == std::string::npos) {
        result.error = "Empty file: no content to parse.";
        return result;
    }

    std::istringstream stream(content);
    std::string line;

    if (!std::getline(stream, line)) {
        result.error = "Empty file: could not read a header row.";
        return result;
    }
    const auto header = split_csv_line(line);
    bool header_ok = header.size() == kExpectedHeader.size();
    if (header_ok) {
        for (std::size_t i = 0; i < header.size(); ++i) {
            if (trim(header[i]) != kExpectedHeader[i]) {
                header_ok = false;
                break;
            }
        }
    }
    if (!header_ok) {
        std::ostringstream expected;
        for (std::size_t i = 0; i < kExpectedHeader.size(); ++i) {
            if (i) expected << ',';
            expected << kExpectedHeader[i];
        }
        result.error = "Invalid or missing schema: header must be exactly '" + expected.str() +
                        "'. Found: '" + line + "'.";
        return result;
    }

    int row_num = 1;  // header is row 1 (1-indexed, matches what a user sees in a spreadsheet)
    while (std::getline(stream, line)) {
        ++row_num;
        if (trim(line).empty()) continue;  // tolerate trailing blank lines

        const auto fields = split_csv_line(line);
        if (fields.size() != kExpectedHeader.size()) {
            result.error = "Row " + std::to_string(row_num) + ": expected " +
                            std::to_string(kExpectedHeader.size()) + " fields (" + "node_id,x_m,y_m,z_m,temperature_C" +
                            ") but found " + std::to_string(fields.size()) + ".";
            result.nodes.clear();
            return result;
        }

        AnsysNode node;
        try {
            std::size_t consumed = 0;

            const std::string id_str = trim(fields[0]);
            node.node_id = std::stol(id_str, &consumed);
            if (consumed != id_str.size()) throw std::invalid_argument("trailing garbage in node_id");

            auto parse_field = [&](const std::string& raw, const char* name) -> double {
                const std::string s = trim(raw);
                std::size_t n = 0;
                double v = std::stod(s, &n);
                if (n != s.size()) {
                    throw std::invalid_argument(std::string(name) + " has trailing garbage: '" + s + "'");
                }
                if (!std::isfinite(v)) {
                    throw std::invalid_argument(std::string(name) + " is not finite (NaN/Inf): '" + s + "'");
                }
                return v;
            };

            node.x = parse_field(fields[1], "x_m");
            node.y = parse_field(fields[2], "y_m");
            node.z = parse_field(fields[3], "z_m");
            node.temperature = parse_field(fields[4], "temperature_C");
        } catch (const std::exception& e) {
            result.error = "Row " + std::to_string(row_num) + ": invalid record -- " + e.what();
            result.nodes.clear();
            return result;
        }

        result.nodes.push_back(node);
    }

    if (result.nodes.empty()) {
        result.error = "CSV has a valid header but zero data rows (empty dataset).";
        return result;
    }

    double sum = 0.0;
    result.min_temperature = result.nodes.front().temperature;
    result.max_temperature = result.nodes.front().temperature;
    for (const auto& n : result.nodes) {
        sum += n.temperature;
        result.min_temperature = std::min(result.min_temperature, n.temperature);
        result.max_temperature = std::max(result.max_temperature, n.temperature);
    }
    result.avg_temperature = sum / static_cast<double>(result.nodes.size());

    result.valid = true;
    result.error.clear();
    return result;
}

}  // namespace nexus::geometry
