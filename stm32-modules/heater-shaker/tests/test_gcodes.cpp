#include <array>

#include "catch2/catch.hpp"
#include "heater-shaker/gcodes.hpp"

SCENARIO("SetRPM parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a fully non-matching string") {
        std::string to_parse = "asdhalghasdasd ";

        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with prefix only") {
        auto to_parse = std::array{'M', '3', ' ', 'S'};

        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "Masdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M3 Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and float data") {
        std::string to_parse = "M3 S1000.0\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and a negative value") {
        std::string to_parse = "M3 S-10\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and positive integral data") {
        std::string to_parse = "M3 S1000\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm == 1000);
                REQUIRE(result.second == to_parse.cbegin() + 8);
            }
        }
    }

    GIVEN("a string with valid data and content afterwards") {
        std::string to_parse = "M3 S1000 asgasasd";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm == 1000);
                AND_THEN(
                    "the iterator should past just past the end of the gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 8);
                }
            }
        }
    }
}

SCENARIO("SetTemperature parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::SetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a fully non-matching string") {
        std::string to_parse = "asdhalghasdasd ";

        WHEN("calling parse") {
            auto result = gcode::SetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with prefix only") {
        auto to_parse = std::array{'M', '1', '0', '4', ' ', 'S'};

        WHEN("calling parse") {
            auto result = gcode::SetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "Masdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M104 Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and a negative value") {
        std::string to_parse = "M104 S-10\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and positive float data") {
        std::string to_parse = "M104 S25.25\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());

            THEN("a value should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().temperature == 25.25);
                REQUIRE(result.second == to_parse.cbegin() + 11);
            }
        }
    }

    GIVEN("a string with a matching prefix and positive integral data") {
        std::string to_parse = "M104 S25\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().temperature == 25);
                REQUIRE(result.second == to_parse.cbegin() + 8);
            }
        }
    }

    GIVEN("a string with valid data and content afterwards") {
        std::string to_parse = "M104 S25.25 asgasasd";
        WHEN("calling parse") {
            auto result = gcode::SetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().temperature == 25.25);
                AND_THEN(
                    "the iterator should point just past the end of the "
                    "gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 11);
                }
            }
        }
    }
}

SCENARIO("GetTemperature parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::GetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a fully non-matching string") {
        std::string to_parse = "asdhalghasdasd ";

        WHEN("calling parse") {
            auto result = gcode::GetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "Masdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result = gcode::GetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode") {
        auto to_parse = std::array{'M', '1', '0', '5', '\r', '\n'};

        WHEN("calling parse") {
            auto result = gcode::GetTemperature::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin() + 4);
            }
        }
    }
}

SCENARIO("GetRPM parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result =
                gcode::GetRPM::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a fully non-matching string") {
        std::string to_parse = "asdhalghasdasd ";

        WHEN("calling parse") {
            auto result =
                gcode::GetRPM::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "Masdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::GetRPM::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode") {
        auto to_parse = std::array{'M', '1', '2', '3', '\r', '\n'};

        WHEN("calling parse") {
            auto result =
                gcode::GetRPM::parse(to_parse.cbegin(), to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin() + 4);
            }
        }
    }
}
