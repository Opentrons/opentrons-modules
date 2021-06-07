#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

/*
** This test case is for all the gcodes basic failure modes. All new gcodes
*should be added here.
**
*/
TEMPLATE_TEST_CASE("gcode basic parsing", "[gcode][parse]", gcode::SetRPM,
                   gcode::SetTemperature, gcode::GetTemperature, gcode::GetRPM,
                   gcode::SetAcceleration, gcode::GetTemperatureDebug,
                   gcode::SetHeaterPIDConstants, gcode::SetHeaterPowerTest,
                   gcode::EnterBootloader, gcode::GetVersion, gcode::Home,
                   gcode::ActuateSolenoid) {
    SECTION("attempting to parse an empty string fails") {
        std::string to_parse = "";
        auto output = TestType::parse(to_parse.cbegin(), to_parse.cend());
        REQUIRE(output.second == to_parse.cbegin());
        REQUIRE(!output.first.has_value());
    }
    SECTION("attempting to parse a subset of the prefix fails") {
        std::string to_parse(TestType::prefix.size() * 2, 'c');
        std::copy(TestType::prefix.cbegin(), TestType::prefix.cend() - 1,
                  to_parse.begin());
        auto output = TestType::parse(to_parse.cbegin(), to_parse.cend());
        REQUIRE(output.second == to_parse.cbegin());
        REQUIRE(!output.first.has_value());
    }
    SECTION("attempting to parse a garbage string fails") {
        std::string to_parse = "ahkajshlkajshasd\n";
        auto output = TestType::parse(to_parse.cbegin(), to_parse.cend());
        REQUIRE(output.second == to_parse.cbegin());
        REQUIRE(!output.first.has_value());
    }
}

/*
** This test case is for successful parsing of gcodes without parameters
*/
TEMPLATE_TEST_CASE("gcodes without parameters parse", "[gcode][parse]",
                   gcode::GetRPM, gcode::GetTemperature,
                   gcode::GetTemperatureDebug, gcode::EnterBootloader,
                   gcode::GetVersion, gcode::Home) {
    SECTION("parsing the full prefix succeeds") {
        auto output =
            TestType::parse(TestType::prefix.cbegin(), TestType::prefix.cend());
        REQUIRE(output.first.has_value());
        REQUIRE(output.second == TestType::prefix.cend());
    }
}

/*
** This test case is for the easy-to-test gcodes that have responses that can be
*generated
** without parameters that just echo back the gcode plus "OK\n". You should add
*any gcode whose
** write_response_into method takes no parameters other than the output
*iterators here. Make
** sure that gcode has a static const char* response element.
*/
TEMPLATE_TEST_CASE("gcode responses without parameters generate",
                   "[gcode][response]", gcode::SetRPM, gcode::SetTemperature,
                   gcode::SetHeaterPowerTest, gcode::SetHeaterPIDConstants,
                   gcode::SetAcceleration, gcode::EnterBootloader, gcode::Home,
                   gcode::ActuateSolenoid) {
    SECTION("responses won't write into zero-size buffers") {
        std::string buffer(10, 'c');
        auto res =
            TestType::write_response_into(buffer.begin(), buffer.begin());
        REQUIRE(res == buffer.begin());
        REQUIRE_THAT(buffer, Catch::Matchers::Equals("cccccccccc"));
    }

    SECTION("responses won't overwrite buffers that are too small") {
        std::string buffer(TestType::response);
        buffer.resize(buffer.size() - 3);
        size_t available = buffer.size();
        buffer.append(10, 'c');
        auto res = TestType::write_response_into(buffer.begin(),
                                                 buffer.begin() + available);
        REQUIRE(res == (buffer.begin() + available));
        std::string prefix(buffer.cbegin(), buffer.cbegin() + available);
        REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(prefix));
        std::string guardstring(10, 'c');
        REQUIRE_THAT(buffer, Catch::Matchers::EndsWith(guardstring));
    }

    SECTION("responses write correctly into buffers that are large enough") {
        std::string buffer(TestType::response);
        auto res = TestType::write_response_into(buffer.begin(), buffer.end());
        REQUIRE(res == buffer.end());
        REQUIRE_THAT(buffer, Catch::Matchers::Equals(TestType::response));
    }

    SECTION(
        "responses correctly indicate remaining space in buffers with margin") {
        std::string buffer(TestType::response);
        auto available = buffer.size();
        std::string margin_suffix(10, 'c');
        buffer += margin_suffix;
        auto res = TestType::write_response_into(buffer.begin(), buffer.end());
        REQUIRE(res == buffer.begin() + available);
        REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(TestType::response));
        REQUIRE_THAT(buffer, Catch::Matchers::EndsWith(margin_suffix));
    }
}
