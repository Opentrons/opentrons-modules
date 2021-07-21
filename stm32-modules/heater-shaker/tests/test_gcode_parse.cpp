#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <utility>

#include "catch2/catch.hpp"
#include "heater-shaker/gcode_parser.hpp"

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
