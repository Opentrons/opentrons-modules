#include <catch2/catch.hpp>
#include <string>

#include "vacuum-module/gcodes.hpp"

TEST_CASE("M127 parses waste knobs", "[gcode][m127]") {
    std::string cmd = "M127 A0.5 G0.5 H8 T6000 U10000 N20 E1\n";
    auto parsed =
        gcode::SetWasteDetectionConfig::parse(cmd.cbegin(), cmd.cend());
    REQUIRE(parsed.first.has_value());
    auto g = parsed.first.value();
    REQUIRE(g.enable_waste_full);
    REQUIRE(*g.p_filter_alpha == Approx(0.5));
    REQUIRE(*g.g_sealed_max == Approx(0.5));
    REQUIRE(*g.flowing_dp_mbar == Approx(8.0));
    REQUIRE(*g.stable_hold_ms == Approx(6000.0));
    REQUIRE(*g.stable_hold_deep_ms == Approx(10000.0));
    REQUIRE(*g.min_waste_depth_mbar == Approx(20.0));
}

TEST_CASE("M127 E0 disables waste detection", "[gcode][m127]") {
    std::string cmd = "M127 E0\n";
    auto parsed =
        gcode::SetWasteDetectionConfig::parse(cmd.cbegin(), cmd.cend());
    REQUIRE(parsed.first.has_value());
    REQUIRE_FALSE(parsed.first.value().enable_waste_full);
    REQUIRE_FALSE(parsed.first.value().g_sealed_max.has_value());
}

TEST_CASE("M128 writes waste knobs", "[gcode][m128]") {
    std::string buffer(256, 'c');
    auto written = gcode::GetWasteDetectionConfig::write_response_into(
        buffer.begin(), buffer.end(), true, 0.5, 0.5, 8.0, 6000.0, 10000.0,
        20.0);
    REQUIRE(written != buffer.begin());
    auto text = std::string(buffer.begin(), written);
    REQUIRE(text == "M128 E:1 A:0.5 G:0.50 H:8.0 T:6000 U:10000 N:20.0 OK\n");
}
