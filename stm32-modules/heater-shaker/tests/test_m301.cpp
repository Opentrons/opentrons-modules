#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("SetPIDConstants (M301) parser works", "[gcode][parse][m301]") {
    GIVEN("a string with prefix only") {
        std::string to_parse = "M301 T\n";

        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad target") {
        std::string to_parse = "M301 Ta\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix and target matching but no p") {
        std::string to_parse = "M301 TH\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix and target matching and P prefix") {
        std::string to_parse = "M301 TH P\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix and target matching and P bad data ") {
        std::string to_parse = "M301 TH Pfaoiuhsda\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p ok and no i or d") {
        std::string to_parse = "M301 P22.1\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p ok and i prefix only") {
        std::string to_parse = "M301 P22.1 I\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p ok and i bad data") {
        std::string to_parse = "M301 TM P22.1 Isaoihdals\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p and i ok and no d") {
        std::string to_parse = "M301 TH P22.1 I22.1\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p and i ok and d prefix only") {
        std::string to_parse = "M301 TM P22.1 I55.1 D\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p and i ok and d bad data") {
        std::string to_parse = "M301 TH P22.1 I55.1 Dasdas\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a correct heater target command") {
        std::string to_parse = "M301 TH P22.1 I0.15 D-1.2\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a value should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().target ==
                        gcode::SetPIDConstants::HEATER);
                REQUIRE_THAT(result.first.value().kp,
                             Catch::Matchers::WithinAbs(22.1, .01));
                REQUIRE_THAT(result.first.value().ki,
                             Catch::Matchers::WithinAbs(0.15, 0.001));
                REQUIRE_THAT(result.first.value().kd,
                             Catch::Matchers::WithinAbs(-1.2, .01));
                REQUIRE(result.second == (to_parse.cend() - 2));
            }
        }
    }
    GIVEN("a correct motor target command") {
        std::string to_parse = "M301 TM P19.1 I1.15 D-4.2\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetPIDConstants::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a value should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().target ==
                        gcode::SetPIDConstants::MOTOR);
                REQUIRE_THAT(result.first.value().kp,
                             Catch::Matchers::WithinAbs(19.1, .01));
                REQUIRE_THAT(result.first.value().ki,
                             Catch::Matchers::WithinAbs(1.15, 0.001));
                REQUIRE_THAT(result.first.value().kd,
                             Catch::Matchers::WithinAbs(-4.2, .01));
                REQUIRE(result.second == (to_parse.cend() - 2));
            }
        }
    }
}
