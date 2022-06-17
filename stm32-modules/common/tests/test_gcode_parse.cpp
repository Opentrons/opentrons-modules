#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <utility>

#include "catch2/catch.hpp"
#include "core/gcode_parser.hpp"

struct G28D2 {
    using ParseResult = std::optional<G28D2>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto code = std::array{'G', '2', '8', '.', '2'};
        if (static_cast<size_t>(limit - input) >= code.size() &&
            std::equal(code.cbegin(), code.cend(), input)) {
            return std::make_pair(ParseResult(G28D2()), input + code.size());
        } else {
            return std::make_pair(ParseResult(), input);
        }
    }
};

struct M105 {
    using ParseResult = std::optional<M105>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        using namespace std::string_literals;
        auto code = std::array{'M', '1', '0', '5'};
        if (static_cast<size_t>(limit - input) >= code.size() &&
            std::equal(code.cbegin(), code.cend(), input)) {
            return std::make_pair(ParseResult(M105()), input + code.size());
        } else {
            return std::make_pair(ParseResult(), input);
        }
    }
};

SCENARIO("GroupParser handles gcodes", "[gcode]") {
    GIVEN("A GroupParser with a couple recognizers") {
        auto parser = gcode::GroupParser<G28D2, M105>();
        AND_GIVEN("an empty string") {
            const std::string empty = "";
            WHEN("calling parse()") {
                auto begin = empty.cbegin();
                auto result = parser.parse_available(begin, empty.cend());

                THEN("The result is empty") {
                    REQUIRE(
                        std::holds_alternative<std::monostate>(result.first));
                    REQUIRE(result.second == empty.cbegin());
                }
            }
        }
        AND_GIVEN("a string with delimieters but no gcode") {
            const std::string with_delimiters = "\r\n";
            WHEN("calling parse()") {
                auto begin = with_delimiters.cbegin();
                auto result =
                    parser.parse_available(begin, with_delimiters.cend());

                THEN("The result is empty") {
                    REQUIRE(
                        std::holds_alternative<std::monostate>(result.first));
                    REQUIRE(result.second == with_delimiters.cend());
                }
            }
        }

        AND_GIVEN("a string with one gcode") {
            const std::string has_gcode = "G28.2\r\n";
            WHEN("calling parse() the first time") {
                auto begin = has_gcode.cbegin();
                auto result = parser.parse_available(begin, has_gcode.cend());
                THEN("the first result is the gcode and parsing remains") {
                    REQUIRE(std::holds_alternative<G28D2>(result.first));
                    REQUIRE(result.second != has_gcode.cend());
                }
                AND_WHEN("calling parse() again") {
                    auto nothing_else =
                        parser.parse_available(result.second, has_gcode.cend());
                    THEN("the second result is empty and parsing is done") {
                        REQUIRE(std::holds_alternative<std::monostate>(
                            nothing_else.first));
                        REQUIRE(nothing_else.second == has_gcode.cend());
                    }
                }
            }
        }

        AND_GIVEN("a string with several gcodes") {
            const std::string multiple_gcodes = "G28.2 M105 G28.2\r\n";
            WHEN("calling parse() the first time") {
                auto begin = multiple_gcodes.cbegin();
                auto first_result =
                    parser.parse_available(begin, multiple_gcodes.cend());
                THEN(
                    "the first result is the first gcode and parsing remains") {
                    REQUIRE(std::holds_alternative<G28D2>(first_result.first));
                    REQUIRE(first_result.second != multiple_gcodes.cend());
                }
                AND_WHEN("calling parse() the second time") {
                    auto second_result = parser.parse_available(
                        first_result.second, multiple_gcodes.cend());
                    THEN(
                        "the second result is the second gcode and parsing "
                        "remains") {
                        REQUIRE(
                            std::holds_alternative<M105>(second_result.first));
                        REQUIRE(second_result.second != multiple_gcodes.cend());
                    }
                    AND_WHEN("calling parse() the third time") {
                        auto third_result = parser.parse_available(
                            second_result.second, multiple_gcodes.cend());
                        THEN(
                            "the third result is the third gcode and parsing "
                            "remains") {
                            REQUIRE(std::holds_alternative<G28D2>(
                                third_result.first));
                            REQUIRE(third_result.second !=
                                    multiple_gcodes.cend());
                        }
                        AND_WHEN("calling parse() the fourth time") {
                            auto no_result = parser.parse_available(
                                third_result.second, multiple_gcodes.cend());
                            THEN(
                                "the fourth result is empty and parsing is "
                                "done") {
                                REQUIRE(std::holds_alternative<std::monostate>(
                                    no_result.first));
                                REQUIRE(no_result.second ==
                                        multiple_gcodes.cend());
                            }
                        }
                    }
                }
            }
        }

        AND_GIVEN("a string with invalid data") {
            const std::string all_invalid = "ajahsdkjahsdf\r\n";
            WHEN("calling parse()") {
                auto begin = all_invalid.cbegin();
                auto first_result =
                    parser.parse_available(begin, all_invalid.cend());
                THEN("the result is an error and parsing is done") {
                    REQUIRE(
                        std::holds_alternative<decltype(parser)::ParseError>(
                            first_result.first));
                    REQUIRE(first_result.second == all_invalid.cend());
                }
            }
        }
    }
}

// Float arg
struct ArgFloat {
    static constexpr auto prefix = std::array{'A'};
    static constexpr bool required = true;
    bool present = false;
    float value = 0.0F;
};

// Int arg
struct ArgInt {
    static constexpr auto prefix = std::array{'H', 'I'};
    static constexpr bool required = true;
    bool present = false;
    int value = 0;
};

// String arg with no prefix
struct ArgString {
    static constexpr bool required = true;
    bool present;
    std::array<char, 30> value;
};

struct ArgFlag {
    static constexpr auto prefix = std::array{'H', 'E', 'Y'};
    static constexpr bool required = true;
    bool present;
};

struct ArgOptional {
    static constexpr auto prefix = std::array{'N'};
    static constexpr bool required = false;
    bool present;
    int value;
};

TEST_CASE("parse_gcode functionality") {
    GIVEN("a gcode without any arguments") {
        constexpr auto prefix = std::array{'M', '1', '2', '3'};
        THEN("parsing a good input succeeds") {
            const std::string input = "M123";
            auto ret = gcode::GcodeParseSingle<>::parse_gcode(
                input.begin(), input.end(), prefix);
            REQUIRE(ret.second == input.end());
            // No inputs = no return on the optional value...
            REQUIRE(!ret.first.has_value());
        }
    }
    GIVEN("a gcode M123 with a flag argument (no value)") {
        constexpr auto prefix = std::array{'M', '1', '2', '3'};
        const std::string input = "M123 HEY";
        THEN("parsing succeeds") {
            auto ret = gcode::GcodeParseSingle<ArgFlag>::parse_gcode(
                input.begin(), input.end(), prefix);
            REQUIRE(ret.first.has_value());
            REQUIRE(ret.second == input.end());
            auto args = ret.first.value();
            REQUIRE(std::get<0>(args).present);
        }
    }
    GIVEN("a gcode M123 with one numeric argument") {
        constexpr auto prefix = std::array{'M', '1', '2', '3'};
        AND_GIVEN("a valid input") {
            const std::string input = "M123 A4.0\n";
            THEN("input can be parsed") {
                auto ret = gcode::GcodeParseSingle<ArgFloat>::parse_gcode(
                    input.begin(), input.end(), prefix);
                REQUIRE(ret.first.has_value());
                REQUIRE(ret.second != input.begin());
                auto args = ret.first.value();
                REQUIRE(std::get<0>(args).value == 4.0F);
                REQUIRE(std::get<0>(args).present);
            }
        }
    }
    GIVEN("a gcode M123 with two numeric arguments") {
        constexpr auto prefix = std::array{'M', '1', '2', '3'};
        AND_GIVEN("a valid input") {
            const std::string input = "M123 A4.0 HI-400\n";
            THEN("input can be parsed") {
                auto ret =
                    gcode::GcodeParseSingle<ArgFloat, ArgInt>::parse_gcode(
                        input.begin(), input.end(), prefix);
                REQUIRE(ret.first.has_value());
                REQUIRE(ret.second != input.begin());
                auto args = ret.first.value();
                REQUIRE(std::get<0>(args).value == 4.0);
                REQUIRE(std::get<0>(args).present);
                REQUIRE(std::get<1>(args).value == -400);
                REQUIRE(std::get<1>(args).present);
            }
        }
    }
    GIVEN("a gcode M123 with an optional argument") {
        constexpr auto prefix = std::array{'M', '1', '2', '3'};
        GIVEN("the optional value is the only argument") {
            THEN("legal inputs succeed") {
                auto input =
                    GENERATE(as<std::string>{}, "M123 \n", "M123 N5  ");
                auto ret = gcode::GcodeParseSingle<ArgOptional>::parse_gcode(
                    input.begin(), input.end(), prefix);
                REQUIRE(ret.first.has_value());
                REQUIRE(ret.second != input.begin());
            }
            THEN("illegal inputs don't succeed") {
                auto input =
                    GENERATE(as<std::string>{}, "M123 Nabcd\n", "M123 N ");
                auto ret = gcode::GcodeParseSingle<ArgOptional>::parse_gcode(
                    input.begin(), input.end(), prefix);
                REQUIRE(!ret.first.has_value());
                REQUIRE(ret.second == input.begin());
            }
        }
        GIVEN("the optional value is the first argument") {
            THEN("legal inputs succeed") {
                auto input = GENERATE(as<std::string>{}, "M123 HI123\n",
                                      "M123 N5  HI123\n");
                auto ret =
                    gcode::GcodeParseSingle<ArgOptional, ArgInt>::parse_gcode(
                        input.begin(), input.end(), prefix);
                REQUIRE(ret.first.has_value());
                REQUIRE(ret.second != input.begin());
                auto argint = std::get<1>(ret.first.value());
                REQUIRE(argint.present);
                REQUIRE(argint.value == 123);
                auto argoptional = std::get<0>(ret.first.value());
                if (argoptional.present) {
                    REQUIRE(argoptional.value == 5);
                }
            }
            THEN("illegal inputs don't succeed") {
                auto input = GENERATE(as<std::string>{}, "M123 N5\n",
                                      "M123 N N HI953\n", "M123 N HI543\n");
                auto ret =
                    gcode::GcodeParseSingle<ArgOptional, ArgInt>::parse_gcode(
                        input.begin(), input.end(), prefix);
                REQUIRE(!ret.first.has_value());
                REQUIRE(ret.second == input.begin());
            }
        }
        GIVEN("the optional value is the second argument") {
            THEN("legal inputs succeed") {
                auto input = GENERATE(as<std::string>{}, "M123 HI123\n",
                                      "M123   HI123  N5\n");
                auto ret =
                    gcode::GcodeParseSingle<ArgInt, ArgOptional>::parse_gcode(
                        input.begin(), input.end(), prefix);
                REQUIRE(ret.first.has_value());
                REQUIRE(ret.second != input.begin());
                auto argint = std::get<0>(ret.first.value());
                REQUIRE(argint.present);
                REQUIRE(argint.value == 123);
                auto argoptional = std::get<1>(ret.first.value());
                if (argoptional.present) {
                    REQUIRE(argoptional.value == 5);
                }
            }
            THEN("illegal inputs don't succeed") {
                auto input = GENERATE(as<std::string>{}, "M123 N5\n",
                                      "M123 N N HI953\n", "M123 N HI543\n");
                auto ret =
                    gcode::GcodeParseSingle<ArgInt, ArgOptional>::parse_gcode(
                        input.begin(), input.end(), prefix);
                REQUIRE(!ret.first.has_value());
                REQUIRE(ret.second == input.begin());
            }
        }
    }
    GIVEN("a gcode with a string argument and a valid input") {
        const std::string input = "M119 ABCDEFG12345\n";
        constexpr auto prefix = std::array{'M', '1', '1', '9'};

        THEN("parsing succeeds") {
            const std::string expected = "ABCDEFG12345";
            auto ret = gcode::GcodeParseSingle<ArgString>::parse_gcode(
                input.begin(), input.end(), prefix);
            REQUIRE(ret.first.has_value());
            REQUIRE(ret.second != input.begin());
            auto val = std::get<0>(ret.first.value());
            REQUIRE(val.value[0] == 'A');
            REQUIRE(expected.compare(val.value.begin()) == 0);
        }
    }
}
