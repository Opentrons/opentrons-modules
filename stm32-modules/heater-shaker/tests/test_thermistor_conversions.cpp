#include <cmath>
#include "catch2/catch.hpp"
#include "heater-shaker/thermistor_conversion.hpp"

using namespace thermistor_conversion;

SCENARIO("thermistor conversion boundary cases") {
    GIVEN("a converter") {
        auto converter = Conversion(ThermistorType::NTCG104ED104DTDSX, 2000, 10);
        WHEN("sending a 0 result") {
            auto converted = converter.convert(0);
            THEN("the result should be off-scale high (less resistance - higher temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_HIGH);
            }
        }
        WHEN("sending a very low but not statically-incorrect result") {
            auto converted = converter.convert(1);
            THEN("the result should be off-scale high (less resistance - higher temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_HIGH);
            }
        }
        WHEN("sending a result equal to adc max") {
            auto converted = converter.convert((1U << 10)-1);
            THEN("the result should be off-scale low (more resistance - lower temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_LOW);
            }
        }
        WHEN("sending a result above valid but not statically-incorrect") {
            auto converted = converter.convert((1U<<10)-2);
            THEN("the result should be off-scale low (more resistance - lower temp)") {
                REQUIRE(std::get<Error>(converted) == Error::OUT_OF_RANGE_LOW);
            }
        }
    }
}

SCENARIO("thermistor conversions normal operation") {
    GIVEN("a converter") {
        auto converter = Conversion(ThermistorType::NTCG104ED104DTDSX, 10000, 10);
        WHEN("sending a valid reading") {
            auto converted = converter.convert(1U << 5);
            THEN("the result should be reasonable") {
                REQUIRE_THAT(std::get<double>(converted), Catch::Matchers::WithinAbs(1.78, .1));
            }
        }
    }
}
