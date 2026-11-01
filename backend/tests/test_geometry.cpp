#include <gtest/gtest.h>
#include "nexus/geometry.hpp"

using namespace nexus::geometry;

// ---------------------------------------------------------------------
// ASCII STL -- positive
// ---------------------------------------------------------------------

TEST(StlParser, ValidTetrahedronParsesWithExpectedGeometry) {
    const std::string stl =
        "solid test\n"
        "  facet normal 0 0 -1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 0 1 0\n"
        "      vertex 1 0 0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0 -1 0\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 0 1\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid test\n";

    const auto r = parse_ascii_stl(stl);
    ASSERT_TRUE(r.valid) << r.error;
    EXPECT_EQ(r.triangles.size(), 2u);
    EXPECT_EQ(r.solid_name, "test");
    EXPECT_DOUBLE_EQ(r.bbox.min.x, 0.0);
    EXPECT_DOUBLE_EQ(r.bbox.max.x, 1.0);
    EXPECT_DOUBLE_EQ(r.bbox.max.z, 1.0);
}

TEST(StlParser, SolidNameMayContainSpaces) {
    const std::string stl =
        "solid my sample part v2\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid\n";
    const auto r = parse_ascii_stl(stl);
    ASSERT_TRUE(r.valid) << r.error;
    EXPECT_EQ(r.solid_name, "my sample part v2");
}

// ---------------------------------------------------------------------
// ASCII STL -- negative (brief section 12: "Empty STL", "Malformed STL",
// "Invalid numeric values")
// ---------------------------------------------------------------------

TEST(StlParser, RejectsEmptyFile) {
    const auto r = parse_ascii_stl("");
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("Empty file"), std::string::npos);
}

TEST(StlParser, RejectsWhitespaceOnlyFile) {
    const auto r = parse_ascii_stl("   \n\n\t  \n");
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("Empty file"), std::string::npos);
}

TEST(StlParser, RejectsSolidHeaderWithNoFacets) {
    const auto r = parse_ascii_stl("solid empty\nendsolid empty\n");
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("zero facets"), std::string::npos);
}

TEST(StlParser, RejectsMalformedMissingEndloop) {
    // Missing 'endloop' before 'endfacet' -- a classic hand-edited-file typo.
    const std::string stl =
        "solid bad\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "  endfacet\n"
        "endsolid bad\n";
    const auto r = parse_ascii_stl(stl);
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("Malformed STL"), std::string::npos);
}

TEST(StlParser, RejectsFacetWithOnlyTwoVertices) {
    const std::string stl =
        "solid bad\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid bad\n";
    const auto r = parse_ascii_stl(stl);
    EXPECT_FALSE(r.valid);
}

TEST(StlParser, RejectsNonFiniteVertexCoordinate) {
    const std::string stl =
        "solid bad\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex nan 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid bad\n";
    const auto r = parse_ascii_stl(stl);
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("not a finite number"), std::string::npos);
}

TEST(StlParser, RejectsGarbageNumericToken) {
    const std::string stl =
        "solid bad\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 1.0x 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid bad\n";
    const auto r = parse_ascii_stl(stl);
    EXPECT_FALSE(r.valid);
}

TEST(StlParser, RejectsBinaryLookingContentAsNotAsciiGrammar) {
    // A binary STL's 80-byte header often isn't literally "solid " but
    // some exporters write misleading text there; either way, feeding
    // raw binary bytes at this ASCII parser should fail the grammar
    // rather than silently "succeed" with garbage triangles.
    std::string fake_binary = "solid ";
    for (int i = 0; i < 200; ++i) fake_binary.push_back(static_cast<char>((i * 37) % 256));
    const auto r = parse_ascii_stl(fake_binary);
    EXPECT_FALSE(r.valid);
}

TEST(StlParser, RejectsOversizedFile) {
    std::string huge = "solid big\n";
    huge.append(200, 'x');  // not valid grammar anyway, but size check runs first
    const auto r = parse_ascii_stl(huge, /*max_bytes=*/50);
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("size limit"), std::string::npos);
}

// ---------------------------------------------------------------------
// ANSYS CSV -- positive
// ---------------------------------------------------------------------

TEST(CsvParser, ValidCsvParsesWithCorrectStats) {
    const std::string csv =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,20.0\n"
        "2,1.0,0.0,0.0,40.0\n"
        "3,0.0,1.0,0.0,60.0\n";
    const auto r = parse_ansys_csv(csv);
    ASSERT_TRUE(r.valid) << r.error;
    EXPECT_EQ(r.nodes.size(), 3u);
    EXPECT_DOUBLE_EQ(r.min_temperature, 20.0);
    EXPECT_DOUBLE_EQ(r.max_temperature, 60.0);
    EXPECT_NEAR(r.avg_temperature, 40.0, 1e-9);
    EXPECT_EQ(r.nodes[1].node_id, 2);
    EXPECT_DOUBLE_EQ(r.nodes[1].x, 1.0);
}

TEST(CsvParser, ToleratesTrailingBlankLines) {
    const std::string csv =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,20.0\n"
        "\n";
    const auto r = parse_ansys_csv(csv);
    ASSERT_TRUE(r.valid) << r.error;
    EXPECT_EQ(r.nodes.size(), 1u);
}

// ---------------------------------------------------------------------
// ANSYS CSV -- negative (brief section 12: "Missing columns", "Invalid
// records", "Non-finite values", "Empty files")
// ---------------------------------------------------------------------

TEST(CsvParser, RejectsEmptyFile) {
    const auto r = parse_ansys_csv("");
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("Empty file"), std::string::npos);
}

TEST(CsvParser, RejectsHeaderOnlyFileAsEmptyDataset) {
    const auto r = parse_ansys_csv("node_id,x_m,y_m,z_m,temperature_C\n");
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("zero data rows"), std::string::npos);
}

TEST(CsvParser, RejectsMissingColumn) {
    // Missing z_m entirely.
    const std::string csv =
        "node_id,x_m,y_m,temperature_C\n"
        "1,0.0,0.0,20.0\n";
    const auto r = parse_ansys_csv(csv);
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("schema"), std::string::npos);
}

TEST(CsvParser, RejectsReorderedColumns) {
    // Right column names, wrong order -- schema requires exact order.
    const std::string csv =
        "x_m,node_id,y_m,z_m,temperature_C\n"
        "0.0,1,0.0,0.0,20.0\n";
    const auto r = parse_ansys_csv(csv);
    EXPECT_FALSE(r.valid);
}

TEST(CsvParser, RejectsRowWithWrongFieldCount) {
    const std::string csv =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0\n";  // missing temperature_C value
    const auto r = parse_ansys_csv(csv);
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("Row 2"), std::string::npos);
}

TEST(CsvParser, RejectsNonNumericField) {
    const std::string csv =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,abc,0.0,0.0,20.0\n";
    const auto r = parse_ansys_csv(csv);
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("Row 2"), std::string::npos);
}

TEST(CsvParser, RejectsNonFiniteTemperature) {
    const std::string csv =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,nan\n";
    const auto r = parse_ansys_csv(csv);
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("not finite"), std::string::npos);
}

TEST(CsvParser, RejectsInfinityValue) {
    const std::string csv =
        "node_id,x_m,y_m,z_m,temperature_C\n"
        "1,0.0,0.0,0.0,inf\n";
    const auto r = parse_ansys_csv(csv);
    EXPECT_FALSE(r.valid);
}

TEST(CsvParser, RejectsOversizedFile) {
    std::string huge = "node_id,x_m,y_m,z_m,temperature_C\n";
    huge.append(200, '1');
    const auto r = parse_ansys_csv(huge, /*max_bytes=*/10);
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("size limit"), std::string::npos);
}
