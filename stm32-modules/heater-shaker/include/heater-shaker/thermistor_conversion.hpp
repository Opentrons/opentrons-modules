#pragma once
#include <cstdint>
#include <variant>

/*
** thermistor_conversion is a general-purpose c++ struct that can be
*parametrized
** with the real-world configuration values of an NTC thermistor circuit. While
*the
** standard library doesn't implement compile time math for a variety of reasons
** mostly having to do with error handling, this struct calculates what it can
*at
** compile time and pushes the rest of the parameter generation to the
*constructor
** to make the actual conversion fast
*/

namespace thermistor_conversion {
enum class ThermistorType {
    NTCG104ED104DTDSX,
};

enum class Error { OUT_OF_RANGE_LOW, OUT_OF_RANGE_HIGH };

struct Conversion {
    using Result = std::variant<double, Error>;
    /*
    ** Build a converter. The resistance should be in kiloohms to match the
    *tbales.
    */
    Conversion(ThermistorType thermistor, double bias_resistance_nominal_kohm,
               uint8_t adc_max_bits);
    Conversion() = delete;

    [[nodiscard]] auto convert(uint16_t adc_reading) const -> Result;

  private:
    const double adc_max;
    const uint16_t adc_max_result;
    const double bias_resistance_kohm;
    [[nodiscard]] auto resistance_from_adc(uint16_t adc_count) const -> Result;
    [[nodiscard]] auto temperature_from_resistance(double resistance) const
        -> Result;
};
};  // namespace thermistor_conversion
