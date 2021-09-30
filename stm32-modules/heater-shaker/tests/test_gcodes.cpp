#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

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

SCENARIO("GetTemperatureDebug parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::GetTemperatureDebug::parse(to_parse.cbegin(),
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
            auto result = gcode::GetTemperatureDebug::parse(to_parse.cbegin(),
                                                            to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "M105asdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result = gcode::GetTemperatureDebug::parse(to_parse.cbegin(),
                                                            to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode") {
        auto to_parse = std::string("M105.D\r\n");

        WHEN("calling parse") {
            auto result = gcode::GetTemperatureDebug::parse(to_parse.cbegin(),
                                                            to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin() + 6);
            }
        }
    }

    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperatureDebug::write_response_into(
                buffer.begin(), buffer.end(), 10.25, 11.25, 12.25, 10, 11, 12,
                true);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M105.D AT10.25 BT11.25 OT12.25 AD10 "
                                         "BD11 OD12 PG1 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperatureDebug::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10.01, 11.2, 41.2, 44, 10,
                4, false);
            THEN("the response should write only up to the available space") {
                std::string response = "M105.Dcccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
}

SCENARIO("SetHeaterPIDConstants parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
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
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
                                                              to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with prefix only") {
        std::string to_parse = "M301 P\n";

        WHEN("calling parse") {
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
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
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
                                                              to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M301 Palsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
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
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
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
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
                                                              to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p ok and i bad data") {
        std::string to_parse = "M301 P22.1 Isaoihdals\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
                                                              to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p and i ok and no d") {
        std::string to_parse = "M301 P22.1 I22.1\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
                                                              to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p and i ok and d prefix only") {
        std::string to_parse = "M301 P22.1 I55.1 D\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
                                                              to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with p and i ok and d bad data") {
        std::string to_parse = "M301 P22.1 I55.1 Dasdas\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
                                                              to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a correct command") {
        std::string to_parse = "M301 P22.1 I0.15 D-1.2\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPIDConstants::parse(to_parse.cbegin(),
                                                              to_parse.cend());

            THEN("a value should be parsed") {
                REQUIRE(result.first.has_value());
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

    GIVEN("a response buffer large enough for the response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetHeaterPIDConstants::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("M301 OK\n"));
                REQUIRE(written == buffer.begin() + 8);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string buffer(10, 'c');
        WHEN("filling response") {
            auto written = gcode::SetHeaterPIDConstants::write_response_into(
                buffer.begin(), buffer.begin() + 5);
            THEN("the response should be written only up to the size") {
                REQUIRE_THAT(buffer, Catch::Matchers::Equals("M301 ccccc"));
                REQUIRE(written == buffer.begin() + 5);
            }
        }
    }
}

SCENARIO("SetHeaterPowerTest parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::SetHeaterPowerTest::parse(to_parse.cbegin(),
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
            auto result = gcode::SetHeaterPowerTest::parse(to_parse.cbegin(),
                                                           to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with prefix only") {
        std::string to_parse = "M104.D S\n";

        WHEN("calling parse") {
            auto result = gcode::SetHeaterPowerTest::parse(to_parse.cbegin(),
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
            auto result = gcode::SetHeaterPowerTest::parse(to_parse.cbegin(),
                                                           to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M104.D Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPowerTest::parse(to_parse.cbegin(),
                                                           to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with good data") {
        std::string to_parse = "M104.D S0.5\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPowerTest::parse(to_parse.cbegin(),
                                                           to_parse.cend());

            THEN("the data should be parsed") {
                REQUIRE(result.first.has_value());
            }
        }
    }

    GIVEN("a response buffer large enough for the response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetHeaterPowerTest::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M104.D OK\n"));
                REQUIRE(written == buffer.begin() + 10);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string buffer(10, 'c');
        WHEN("filling response") {
            auto written = gcode::SetHeaterPowerTest::write_response_into(
                buffer.begin(), buffer.begin() + 5);
            THEN("the response should be written only up to the size") {
                REQUIRE_THAT(buffer, Catch::Matchers::Equals("M104.ccccc"));
                REQUIRE(written == buffer.begin() + 5);
            }
        }
    }
}

SCENARIO("dfu parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::EnterBootloader::parse(to_parse.cbegin(),
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
            auto result = gcode::EnterBootloader::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "dfasdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result = gcode::EnterBootloader::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode") {
        auto to_parse = std::string("dfu\r\n");

        WHEN("calling parse") {
            auto result = gcode::EnterBootloader::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin() + 3);
            }
        }
    }

    GIVEN("a response buffer large enough for the response") {
        std::string response_buf(64, 'c');
        WHEN("filling the response") {
            auto written = gcode::EnterBootloader::write_response_into(
                response_buf.begin(), response_buf.end());
            THEN("the response should be filled") {
                REQUIRE_THAT(response_buf,
                             Catch::Matchers::StartsWith("dfu OK\n"));
                REQUIRE(written == response_buf.begin() + 7);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string response_buf(10, 'c');
        auto written = gcode::EnterBootloader::write_response_into(
            response_buf.begin(), response_buf.begin() + 3);
        THEN("the response should be filled only up to its size") {
            REQUIRE_THAT(response_buf, Catch::Matchers::Equals("dfuccccccc"));
            REQUIRE(written == response_buf.begin() + 3);
        }
    }
}

SCENARIO("SetSerialNumber parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
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
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with prefix only") {
        auto to_parse = std::array{'M', '9', '9', '6', ' '};

        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
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
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M996 alsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and float data") {
        std::string to_parse = "M996 1000.0\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and a negative value") {
        std::string to_parse = "M996 -10\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().serial_number == -10);
                REQUIRE(result.second == to_parse.cbegin() + 8);
            }
        }
    }

    GIVEN("a string with a matching prefix and positive integral data") {
        std::string to_parse = "M996 1000\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().serial_number == 1000);
                REQUIRE(result.second == to_parse.cbegin() + 9);
            }
        }
    }

    GIVEN("a string with valid data and content afterwards") {
        std::string to_parse = "M996 1000 asgasasd";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().serial_number == 1000);
                AND_THEN(
                    "the iterator should past just past the end of the gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 9);
                }
            }
        }
    }

    GIVEN("a response buffer large enough for the response") {
        std::string response_buf(64, 'c');
        WHEN("filling the response") {
            auto written = gcode::SetSerialNumber::write_response_into(
                response_buf.begin(), response_buf.end());
            THEN("the response should be filled") {
                REQUIRE_THAT(response_buf,
                             Catch::Matchers::StartsWith("M996 OK\n"));
                REQUIRE(written == response_buf.begin() + 8);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string response_buf(10, 'c');
        auto written = gcode::SetSerialNumber::write_response_into(
            response_buf.begin(), response_buf.begin() + 5);
        THEN("the response should be filled only up to its size") {
            REQUIRE_THAT(response_buf, Catch::Matchers::Equals("M996 ccccc"));
            REQUIRE(written == response_buf.begin() + 5);
        }
    }
}

SCENARIO("OpenPlateLock parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result =
                gcode::OpenPlateLock::parse(to_parse.cbegin(), to_parse.cend());
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
                gcode::OpenPlateLock::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "M24asdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::OpenPlateLock::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode") {
        auto to_parse = std::string("M242\r\n");

        WHEN("calling parse") {
            auto result =
                gcode::OpenPlateLock::parse(to_parse.cbegin(), to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin() + 4);
            }
        }
    }

    GIVEN("a response buffer large enough for the response") {
        std::string response_buf(64, 'c');
        WHEN("filling the response") {
            auto written = gcode::OpenPlateLock::write_response_into(
                response_buf.begin(), response_buf.end());
            THEN("the response should be filled") {
                REQUIRE_THAT(response_buf,
                             Catch::Matchers::StartsWith("M242 OK\n"));
                REQUIRE(written == response_buf.begin() + 8);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string response_buf(10, 'c');
        auto written = gcode::OpenPlateLock::write_response_into(
            response_buf.begin(), response_buf.begin() + 4);
        THEN("the response should be filled only up to its size") {
            REQUIRE_THAT(response_buf, Catch::Matchers::Equals("M242ccccccc"));
            REQUIRE(written == response_buf.begin() + 4);
        }
    }
}

SCENARIO("ClosePlateLock parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::ClosePlateLock::parse(to_parse.cbegin(),
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
            auto result = gcode::ClosePlateLock::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "M24asdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result = gcode::ClosePlateLock::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode") {
        auto to_parse = std::string("M243\r\n");

        WHEN("calling parse") {
            auto result = gcode::ClosePlateLock::parse(to_parse.cbegin(),
                                                       to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin() + 4);
            }
        }
    }

    GIVEN("a response buffer large enough for the response") {
        std::string response_buf(64, 'c');
        WHEN("filling the response") {
            auto written = gcode::ClosePlateLock::write_response_into(
                response_buf.begin(), response_buf.end());
            THEN("the response should be filled") {
                REQUIRE_THAT(response_buf,
                             Catch::Matchers::StartsWith("M243 OK\n"));
                REQUIRE(written == response_buf.begin() + 8);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string response_buf(10, 'c');
        auto written = gcode::ClosePlateLock::write_response_into(
            response_buf.begin(), response_buf.begin() + 4);
        THEN("the response should be filled only up to its size") {
            REQUIRE_THAT(response_buf, Catch::Matchers::Equals("M243ccccccc"));
            REQUIRE(written == response_buf.begin() + 4);
        }
    }
}

SCENARIO("GetPlateLockState parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::GetPlateLockState::parse(to_parse.cbegin(),
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
            auto result = gcode::GetPlateLockState::parse(to_parse.cbegin(),
                                                          to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "M24asdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result = gcode::GetPlateLockState::parse(to_parse.cbegin(),
                                                          to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode") {
        auto to_parse = std::string("M241\r\n");

        WHEN("calling parse") {
            auto result = gcode::GetPlateLockState::parse(to_parse.cbegin(),
                                                          to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin() + 4);
            }
        }
    }

    GIVEN("a response buffer large enough for the response") {
        std::string buffer(64, 'c');
        WHEN("filling the response") {
            auto written = gcode::GetPlateLockState::write_response_into(
                buffer.begin(), buffer.end(),
                std::array<char, 14>{"IDLE_UNKNOWN"});
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M241 STATE:IDLE_UNKNOWN OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string buffer(10, 'c');
        WHEN("filling the response") {
            auto written = gcode::GetPlateLockState::write_response_into(
                buffer.begin(), buffer.begin() + 4,
                std::array<char, 14>{"IDLE_UNKNOWN"});
            THEN("the response should write only up to the available space") {
                std::string response = "M241cccccc";
                response.at(4) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
}

SCENARIO("GetPlateLockStateDebug parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::GetPlateLockStateDebug::parse(
                to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a fully non-matching string") {
        std::string to_parse = "asdhalghasdasd ";

        WHEN("calling parse") {
            auto result = gcode::GetPlateLockStateDebug::parse(
                to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "M2asdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result = gcode::GetPlateLockStateDebug::parse(
                to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode") {
        auto to_parse = std::string("M241.D\r\n");

        WHEN("calling parse") {
            auto result = gcode::GetPlateLockStateDebug::parse(
                to_parse.cbegin(), to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin() + 6);
            }
        }
    }

    GIVEN("a response buffer large enough for the response") {
        std::string buffer(64, 'c');
        WHEN("filling the response") {
            auto written = gcode::GetPlateLockStateDebug::write_response_into(
                buffer.begin(), buffer.end(),
                std::array<char, 14>{"IDLE_UNKNOWN"}, true, true);
            THEN("the response should be filled") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M241.D STATE:IDLE_UNKNOWN "
                                         "OpenSensor:1 ClosedSensor:1 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string buffer(10, 'c');
        WHEN("filling the response") {
            auto written = gcode::GetPlateLockStateDebug::write_response_into(
                buffer.begin(), buffer.begin() + 6,
                std::array<char, 14>{"IDLE_UNKNOWN"}, true, true);
            THEN("the response should write only up to the available space") {
                std::string response = "M241.Dcccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
}
