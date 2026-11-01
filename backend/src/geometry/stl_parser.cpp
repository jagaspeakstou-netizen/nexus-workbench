#include "nexus/geometry.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace nexus::geometry {

namespace {

// Minimal hand-rolled tokenizer that also tracks line numbers, so parse
// errors can point at a specific line -- an istringstream-only approach
// (operator>>) loses that, and "malformed STL" is one of the required
// negative test cases where a useful error message matters.
class Tokenizer {
public:
    explicit Tokenizer(const std::string& text) : text_(text) {}

    // Returns the next whitespace-delimited token, or std::nullopt at EOF.
    std::optional<std::string> next() {
        skip_whitespace();
        if (pos_ >= text_.size()) return std::nullopt;
        std::size_t start = pos_;
        while (pos_ < text_.size() && !std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
        return text_.substr(start, pos_ - start);
    }

    int line() const { return line_; }

private:
    void skip_whitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            if (text_[pos_] == '\n') ++line_;
            ++pos_;
        }
    }

    const std::string& text_;
    std::size_t pos_ = 0;
    int line_ = 1;
};

std::optional<double> parse_double(const std::string& tok) {
    try {
        std::size_t consumed = 0;
        double v = std::stod(tok, &consumed);
        if (consumed != tok.size()) return std::nullopt;  // trailing garbage, e.g. "1.0x"
        if (!std::isfinite(v)) return std::nullopt;        // NaN / Inf
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace

StlParseResult parse_ascii_stl(const std::string& content, std::size_t max_bytes) {
    StlParseResult result;

    if (content.size() > max_bytes) {
        result.error = "File exceeds the " + std::to_string(max_bytes / (1024 * 1024)) +
                        " MiB size limit for ASCII STL uploads.";
        return result;
    }
    if (content.find_first_not_of(" \t\r\n") == std::string::npos) {
        result.error = "Empty file: no content to parse.";
        return result;
    }

    Tokenizer tok(content);

    auto expect = [&](const std::string& want) -> std::optional<std::string> {
        auto t = tok.next();
        if (!t) {
            result.error = "Unexpected end of file (line " + std::to_string(tok.line()) +
                            "): expected '" + want + "'.";
            return std::nullopt;
        }
        if (*t != want) {
            result.error = "Malformed STL at line " + std::to_string(tok.line()) + ": expected '" +
                            want + "' but found '" + *t + "'.";
            return std::nullopt;
        }
        return t;
    };

    auto expect_double = [&](const char* field_name) -> std::optional<double> {
        auto t = tok.next();
        if (!t) {
            result.error = "Unexpected end of file (line " + std::to_string(tok.line()) +
                            "): expected a numeric value for " + field_name + ".";
            return std::nullopt;
        }
        auto v = parse_double(*t);
        if (!v) {
            result.error = "Invalid numeric value for " + std::string(field_name) + " at line " +
                            std::to_string(tok.line()) + ": '" + *t +
                            "' (not a finite number -- NaN/Inf and malformed numbers are rejected).";
            return std::nullopt;
        }
        return v;
    };

    // "solid <name...>" -- name may be empty or contain spaces; consume
    // tokens on this line as the name until something non-name-ish turns
    // up. We keep it simple: read tokens until we see "facet" or
    // "endsolid" (the two only things allowed to open the next section).
    if (!expect("solid")) return result;
    {
        std::ostringstream name;
        while (true) {
            auto t = tok.next();
            if (!t) {
                result.error = "Unexpected end of file: STL has a 'solid' header but no 'facet' "
                                "or 'endsolid' content.";
                return result;
            }
            if (*t == "facet" || *t == "endsolid") {
                // put back by re-tokenizing isn't supported by this simple
                // tokenizer, so handle the two cases inline below instead.
                result.solid_name = name.str();
                if (!result.solid_name.empty() && result.solid_name.back() == ' ') {
                    result.solid_name.pop_back();
                }
                if (*t == "endsolid") {
                    // Header immediately followed by endsolid: zero triangles.
                    result.error = "STL contains a 'solid' header but zero facets (empty geometry).";
                    return result;
                }
                goto parse_facets;  // NOLINT -- simplest way out of a hand-rolled parser loop
            }
            if (!name.str().empty()) name << ' ';
            name << *t;
        }
    }

parse_facets:
    while (true) {
        // The first iteration's "facet" token was already consumed by the
        // header-parsing loop above (it's what triggered this goto); every
        // subsequent iteration consumes its own "facet" token at the loop's
        // tail below before looping back here.
        // Parse one facet: normal nx ny nz / outer loop / vertex x3 / endloop / endfacet
        if (!expect("normal")) return result;
        auto nx = expect_double("normal.x"); if (!nx) return result;
        auto ny = expect_double("normal.y"); if (!ny) return result;
        auto nz = expect_double("normal.z"); if (!nz) return result;

        if (!expect("outer")) return result;
        if (!expect("loop")) return result;

        Triangle tri;
        tri.normal = {*nx, *ny, *nz};

        Vec3* verts[3] = {&tri.v0, &tri.v1, &tri.v2};
        for (int i = 0; i < 3; ++i) {
            if (!expect("vertex")) return result;
            auto x = expect_double("vertex.x"); if (!x) return result;
            auto y = expect_double("vertex.y"); if (!y) return result;
            auto z = expect_double("vertex.z"); if (!z) return result;
            *verts[i] = {*x, *y, *z};
        }

        if (!expect("endloop")) return result;
        if (!expect("endfacet")) return result;

        result.triangles.push_back(tri);

        auto t = tok.next();
        if (!t) {
            result.error = "Unexpected end of file (line " + std::to_string(tok.line()) +
                            "): missing 'endsolid'.";
            return result;
        }
        if (*t == "endsolid") {
            break;  // consume rest of line (name) -- not required for validity
        }
        if (*t != "facet") {
            result.error = "Malformed STL at line " + std::to_string(tok.line()) +
                            ": expected 'facet' or 'endsolid' but found '" + *t + "'.";
            return result;
        }
        // loop continues to parse the next facet
    }

    if (result.triangles.empty()) {
        result.error = "STL parsed successfully but contains zero triangles (empty geometry).";
        return result;
    }

    // Bounding box
    result.bbox.min = result.triangles.front().v0;
    result.bbox.max = result.triangles.front().v0;
    auto expand = [&](const Vec3& v) {
        result.bbox.min.x = std::min(result.bbox.min.x, v.x);
        result.bbox.min.y = std::min(result.bbox.min.y, v.y);
        result.bbox.min.z = std::min(result.bbox.min.z, v.z);
        result.bbox.max.x = std::max(result.bbox.max.x, v.x);
        result.bbox.max.y = std::max(result.bbox.max.y, v.y);
        result.bbox.max.z = std::max(result.bbox.max.z, v.z);
    };
    for (const auto& t : result.triangles) {
        expand(t.v0);
        expand(t.v1);
        expand(t.v2);
    }

    result.valid = true;
    result.error.clear();
    return result;
}

}  // namespace nexus::geometry
