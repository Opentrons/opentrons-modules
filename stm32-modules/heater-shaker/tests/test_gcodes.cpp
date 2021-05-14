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
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm == -10);
                REQUIRE(result.second == to_parse.cbegin() + 7);
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

    GIVEN("a response buffer large enough for the response") {
        std::string response_buf(64, 'c');
        WHEN("filling the response") {
            auto written = gcode::SetRPM::write_response_into(
                response_buf.begin(), response_buf.end());
            THEN("the response should be filled") {
                REQUIRE_THAT(response_buf,
                             Catch::Matchers::StartsWith("M3 OK\n"));
                REQUIRE(written == response_buf.begin() + 6);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string response_buf(10, 'c');
        auto written = gcode::SetRPM::write_response_into(
            response_buf.begin(), response_buf.begin() + 3);
        THEN("the response should be filled only up to its size") {
            REQUIRE_THAT(response_buf, Catch::Matchers::Equals("M3 ccccccc"));
            REQUIRE(written == response_buf.begin() + 3);
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

    GIVEN("a response buffer large enough for the response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetTemperature::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("M104 OK\n"));
                REQUIRE(written == buffer.begin() + 8);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string buffer(10, 'c');
        WHEN("filling response") {
            auto written = gcode::SetTemperature::write_response_into(
                buffer.begin(), buffer.begin() + 5);
            THEN("the response should be written only up to the size") {
                REQUIRE_THAT(buffer, Catch::Matchers::Equals("M104 ccccc"));
                REQUIRE(written == buffer.begin() + 5);
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

    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperature::write_response_into(
                buffer.begin(), buffer.end(), 10.25, 25.001);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M105 C10.25 T25.00 OK\n"));
                REQUIRE(written == buffer.begin() + 22);
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperature::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10, 25);
            THEN("the response should write only up to the available space") {
                std::string response = "M105 Ccccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
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

    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::GetRPM::write_response_into(
                buffer.begin(), buffer.end(), 10, 25);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M123 C10 T25 OK\n"));
                REQUIRE(written == buffer.begin() + 16);
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetRPM::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10, 25);
            THEN("the response should write only up to the available space") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::Equals("M123 Ccccccccccc"));
                REQUIRE(written == buffer.begin() + 7);
            }
        }
    }
}

SCENARIO("SetAcceleration parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
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
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with prefix only") {
        auto to_parse = std::array{'M', '1', '2', '4', ' ', 'S'};

        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
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
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M204 Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and float data") {
        std::string to_parse = "M204 S1000.0\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and a negative value") {
        std::string to_parse = "M204 S-10\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm_per_s == -10);
                REQUIRE(result.second == to_parse.cbegin() + 9);
            }
        }
    }

    GIVEN("a string with a matching prefix and positive integral data") {
        std::string to_parse = "M204 S1000\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm_per_s == 1000);
                REQUIRE(result.second == to_parse.cbegin() + 10);
            }
        }
    }

    GIVEN("a string with valid data and content afterwards") {
        std::string to_parse = "M204 S1000 asgasasd";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm_per_s == 1000);
                AND_THEN(
                    "the iterator should past just past the end of the gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 10);
                }
            }
        }
    }

    GIVEN("a response buffer large enough for the response") {
        std::string response_buf(64, 'c');
        WHEN("filling the response") {
            auto written = gcode::SetAcceleration::write_response_into(
                response_buf.begin(), response_buf.end());
            THEN("the response should be filled") {
                REQUIRE_THAT(response_buf,
                             Catch::Matchers::StartsWith("M204 OK\n"));
                REQUIRE(written == response_buf.begin() + 8);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string response_buf(10, 'c');
        auto written = gcode::SetAcceleration::write_response_into(
            response_buf.begin(), response_buf.begin() + 3);
        THEN("the response should be filled only up to its size") {
            REQUIRE_THAT(response_buf, Catch::Matchers::Equals("M20ccccccc"));
            REQUIRE(written == response_buf.begin() + 3);
        }
    }
}
