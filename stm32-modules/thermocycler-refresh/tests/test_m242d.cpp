#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-refresh/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("gcode m242.d works", "[gcode][parse][m242d]") {
	auto reg = tmc2130::DriveStatus{.sg_result = 123, .stallguard = 1};
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::GetSealDriveStatus::write_response_into(
                buffer.begin(), buffer.end(), reg);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M242.D SG:1 SG_Result:123 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetSealDriveStatus::write_response_into(
                buffer.begin(), buffer.begin() + 8, reg);
            THEN("the response should write only up to the available space") {
                std::string response = "M242.D Scccccccc";
				response.at(7) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
	GIVEN("correct input") {
		std::string input("M242.D\n");
		WHEN("parsing the command") {
			auto parsed = gcode::GetSealDriveStatus::parse(input.begin(), input.end());
			THEN("the command should be correct") {
				REQUIRE(parsed.second != input.begin());
				REQUIRE(parsed.first.has_value());
			}
		}
	}
	GIVEN("incorrect input") {
		std::string input("M242.E \n");
		WHEN("parsing the command") {
			auto parsed = gcode::GetSealDriveStatus::parse(input.begin(), input.end());
			THEN("the command should be incrorrect") {
				REQUIRE(parsed.second == input.begin());
				REQUIRE(!parsed.first.has_value());
			}
		}
	}
}
