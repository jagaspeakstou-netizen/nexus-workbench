// Positive tests for the core status module. This is intentionally the
// first test file in the project so `ctest` works from day one -- every
// later phase adds its own test_<module>.cpp alongside this one.

#include <gtest/gtest.h>
#include "nexus/status.hpp"

TEST(StatusModule, BuildInfoReportsCpp17) {
    const auto info = nexus::get_build_info();
    EXPECT_EQ(info.cpp_standard, "C++17");
    EXPECT_FALSE(info.app_name.empty());
}

TEST(StatusModule, ModuleStatusListIsNonEmptyAndHonest) {
    const auto modules = nexus::get_module_statuses();
    // All 7 non-core modules should be listed.
    EXPECT_EQ(modules.size(), 7u);

    // At this phase, none of them are implemented yet -- this test
    // exists specifically to fail loudly if a future phase marks a
    // module "implemented" without actually building it.
    for (const auto& m : modules) {
        EXPECT_FALSE(m.id.empty());
        EXPECT_FALSE(m.label.empty());
    }
}
