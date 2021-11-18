#include <cmath>

#include "catch2/catch.hpp"
#include "core/thermistor_conversion.hpp"
#include "thermistor_lookups.hpp"

using namespace thermistor_conversion;

SCENARIO("thermistor conversion boundary cases") {
    GIVEN("a converter") {
        auto converter = Conversion<lookups::NTCG104ED104DTDSX>(2000, 10);
        WHEN("sending a 0 result") {
            auto converted = converter.convert(0);
            THEN(
                "the result should be off-scale high (less resistance - higher "
                "temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_HIGH);
            }
        }
        WHEN("sending a very low but not statically-incorrect result") {
            auto converted = converter.convert(1);
            THEN(
                "the result should be off-scale high (less resistance - higher "
                "temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_HIGH);
            }
        }
        WHEN("sending a result equal to adc max") {
            auto converted = converter.convert((1U << 10) - 1);
            THEN(
                "the result should be off-scale low (more resistance - lower "
                "temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_LOW);
            }
        }
        WHEN("sending a result above valid but not statically-incorrect") {
            auto converted = converter.convert((1U << 10) - 2);
            THEN(
                "the result should be off-scale low (more resistance - lower "
                "temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_LOW);
            }
        }
    }
    GIVEN("a KS103J2 converter") {
        auto converter = Conversion<lookups::KS103J2G>(10000, 0x5DC0, false);
        WHEN("sending a 0 result") {
            auto converted = converter.convert(0);
            THEN(
                "the result should be off-scale high (less resistance - higher "
                "temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_HIGH);
            }
        }
        WHEN("sending a very low but not statically-incorrect result") {
            auto converted = converter.convert(1);
            THEN(
                "the result should be off-scale high (less resistance - higher "
                "temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_HIGH);
            }
        }
        WHEN("sending a result equal to adc max") {
            auto converted = converter.convert(0x5DC0);
            THEN(
                "the result should be off-scale low (more resistance - lower "
                "temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_LOW);
            }
        }
        WHEN("sending a result above valid but not statically-incorrect") {
            auto converted = converter.convert(0x5DC0 - 1);
            THEN(
                "the result should be off-scale low (more resistance - lower "
                "temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_LOW);
            }
        }
    }
}

SCENARIO("thermistor conversions normal operation") {
    GIVEN("a converter") {
        auto converter = Conversion<lookups::NTCG104ED104DTDSX>(10000, 10);
        WHEN("sending a valid reading") {
            auto converted = converter.convert(1U << 5);
            THEN("the result should be reasonable") {
                REQUIRE_THAT(std::get<double>(converted),
                             Catch::Matchers::WithinAbs(1.78, .1));
            }
        }
    }
    GIVEN("a KS103J2 converter") {
        // At temp 50C, resistance is 3602.00 ohms, which converts to
        // a voltage of 0.3972 volts, which is ADC 6360
        auto converter = Conversion<lookups::KS103J2G>(10000, 0x5DC0, false);
        WHEN("sending a valid reading") {
            auto converted = converter.convert(6360);
            THEN("the result should be reasonable") {
                REQUIRE_THAT(std::get<double>(converted),
                             Catch::Matchers::WithinAbs(50, .1));
            }
        }
    }
}

SCENARIO("thermistor backconversion") {
    GIVEN("a converter and some test temps") {
        auto converter = Conversion<lookups::NTCG104ED104DTDSX>(1000, 10);
        auto test_vals = std::array{10.0, 25.0, 50.0, 70.0, 90.0};
        for (auto val : test_vals) {
            WHEN("through-converting a temperature") {
                auto reading = converter.backconvert(val);
                THEN("the conversion should be similar") {
                    REQUIRE_THAT(std::get<double>(converter.convert(reading)),
                                 Catch::Matchers::WithinAbs(val, 0.1));
                }
            }
        }
    }
    GIVEN("a KS103J2 converter and some test temps") {
        auto converter = Conversion<lookups::KS103J2G>(10000, 0x5DC0, false);
        auto test_vals = std::array{10.0, 25.0, 50.0, 70.0, 90.0, 100.0};
        for (auto val : test_vals) {
            WHEN("through-converting a temperature") {
                auto reading = converter.backconvert(val);
                THEN("the conversion should be similar") {
                    REQUIRE_THAT(std::get<double>(converter.convert(reading)),
                                 Catch::Matchers::WithinAbs(val, 0.1));
                }
            }
        }
    }
}
