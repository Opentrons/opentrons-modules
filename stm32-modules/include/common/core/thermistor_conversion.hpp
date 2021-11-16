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
enum class ThermistorType { NTCG104ED104DTDSX, KS103J2G };

enum class Error { OUT_OF_RANGE_LOW, OUT_OF_RANGE_HIGH };

enum class TableError { TABLE_END, TABLE_CBEGIN };

struct Conversion {
    using Result = std::variant<double, Error>;
    // First is resistance, second is temperature
    using TableEntry = std::pair<double, int16_t>;
    /** First is After, second is Before (after - 1) */
    using TableEntryPair = std::pair<TableEntry, TableEntry>;
    using TableResult = std::variant<TableEntryPair, TableError>;
    /**
     * Build a converter. The resistance should be in kiloohms to match the
     * tables.
     */
    Conversion(ThermistorType thermistor, double bias_resistance_nominal_kohm,
               uint8_t adc_max_bits);
    /**
     * This initializer builds a converter with a literal bitmap of the
     * max ADC values instead of the number of bits - required when the
     * max voltage of the circuit doesn't match with the ref voltage.
     *
     * NOTE - the param is_signed is ignored for now, but is useful to
     * force differentiation between constructors.
     */
    Conversion(ThermistorType thermistor, double bias_resistance_nominal_kohm,
               uint16_t adc_max_value, bool is_signed);
    Conversion() = delete;

    [[nodiscard]] auto convert(uint16_t adc_reading) const -> Result;
    [[nodiscard]] auto backconvert(double temperature) const -> uint16_t;

  private:
    const double _adc_max;
    const uint16_t _adc_max_result;
    const double _bias_resistance_kohm;
    const ThermistorType _type;
    [[nodiscard]] auto resistance_from_adc(uint16_t adc_count) const -> Result;
    [[nodiscard]] auto temperature_from_resistance(double resistance) const
        -> Result;
    /**
     * Looks for the first table entry with a resistance GREATER than the input
     */
    [[nodiscard]] auto resistance_table_lookup(double resistance) const
        -> TableResult;
    /**
     * Looks for the first table entry with a temperature LESS THAN than the
     * input
     */
    [[nodiscard]] auto temperature_table_lookup(double temperature) const
        -> TableResult;
};
};  // namespace thermistor_conversion
