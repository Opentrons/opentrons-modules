#pragma once
#include <cstdint>
#include <variant>

/** 
 * @file thermistor_conversion.hpp
 * @details
 * thermistor_conversion is a general-purpose c++ struct that can be
 * parametrized with the real-world configuration values of an NTC
 * thermistor circuit. While the standard library doesn't implement compile
 * time math for a variety of reasons mostly having to do with error handling,
 * this struct calculates what it can at compile time and pushes the rest of
 * the parameter generation to the constructor to make the actual conversion
 * fast.
 * 
 * This class is meant to calculate the resistance of the \e second resistor
 * in a resistor ladder, AKA the resistor connected to ground. The 
 * \c Conversion constructor assumes that the specified max ADC reading
 * equates to an R2 value of infinity, while an ADC reading of 0 would
 * equate to a shorted R2. The actual maximum voltage of the circuit doesn't
 * matter.
 */

namespace thermistor_conversion {

enum class Error { OUT_OF_RANGE_LOW, OUT_OF_RANGE_HIGH };

enum class TableError { TABLE_END, TABLE_CBEGIN };

// Conversion can be templatized, and that template type needs to
// be able to return the thermistor table
template <typename GetTable>
concept ThermistorTableT = requires() {
    // Must be a functor that returns an iterable table
    {GetTable()().cbegin()};
    {GetTable()().end()};
};

template <ThermistorTableT GetTable>
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
    Conversion(double bias_resistance_nominal_kohm, uint8_t adc_max_bits)
        : _adc_max(static_cast<double>((1U << adc_max_bits) - 1)),
          _adc_max_result(static_cast<uint16_t>(
              static_cast<uint32_t>(1U << adc_max_bits) - 1)),
          _bias_resistance_kohm(bias_resistance_nominal_kohm) {}
    /**
     * This initializer builds a converter with a literal bitmap of the
     * max ADC values instead of the number of bits - required when the
     * max voltage of the circuit doesn't match with the ref voltage.
     *
     * NOTE - the param is_signed is ignored for now, but is useful to
     * force differentiation between constructors.
     */
    Conversion(double bias_resistance_nominal_kohm, uint16_t adc_max_value,
               bool is_signed)
        : _adc_max(static_cast<double>(adc_max_value)),
          _adc_max_result(
              static_cast<uint16_t>(static_cast<uint32_t>(adc_max_value))),
          _bias_resistance_kohm(bias_resistance_nominal_kohm) {
              static_cast<void>(is_signed);
          }

    Conversion() = delete;

    [[nodiscard]] auto convert(uint16_t adc_reading) const -> Result {
        auto resistance = resistance_from_adc(adc_reading);
        if (std::holds_alternative<Error>(resistance)) {
            return resistance;
        }
        return temperature_from_resistance(std::get<double>(resistance));
    }

    [[nodiscard]] auto backconvert(double temperature) const -> uint16_t {
        auto entries = temperature_table_lookup(temperature);
        if (std::holds_alternative<TableError>(entries)) {
            if (std::get<TableError>(entries) == TableError::TABLE_END) {
                return _adc_max_result;
            }
            return 0;
        }
        auto entry_pair = std::get<TableEntryPair>(entries);

        auto after_temp = static_cast<double>(entry_pair.first.second);
        auto after_res = static_cast<double>(entry_pair.first.first);
        auto before_temp = static_cast<double>(entry_pair.second.second);
        auto before_res = static_cast<double>(entry_pair.second.first);
        double resistance =
            ((after_res - before_res) / (after_temp - before_temp)) *
                (temperature - before_temp) +
            before_res;
        return static_cast<uint16_t>(
            _adc_max / ((_bias_resistance_kohm / resistance) + 1.0));
    }

  private:
    const double _adc_max;
    const uint16_t _adc_max_result;
    const double _bias_resistance_kohm;

    [[nodiscard]] auto resistance_from_adc(uint16_t adc_count) const -> Result {
        if (adc_count >= _adc_max_result) {
            return Result(Error::OUT_OF_RANGE_LOW);
        }
        if (adc_count == 0) {
            return Result(Error::OUT_OF_RANGE_HIGH);
        }
        return Result(_bias_resistance_kohm /
                      ((_adc_max / static_cast<double>(adc_count)) - 1.0));
    }

    [[nodiscard]] auto temperature_from_resistance(double resistance) const
        -> Result {
        auto entries = resistance_table_lookup(resistance);
        if (std::holds_alternative<TableError>(entries)) {
            if (std::get<TableError>(entries) == TableError::TABLE_END) {
                return Result(Error::OUT_OF_RANGE_HIGH);
            }
            return Result(Error::OUT_OF_RANGE_LOW);
        }
        auto entry_pair = std::get<TableEntryPair>(entries);

        auto after_temp = static_cast<double>(entry_pair.first.second);
        auto after_res = entry_pair.first.first;
        auto before_temp = static_cast<double>(entry_pair.second.second);
        auto before_res = entry_pair.second.first;

        return Result((after_temp - before_temp) / (after_res - before_res) *
                          (resistance - before_res) +
                      before_temp);
    }
    /**
     * Looks for the first table entry with a resistance GREATER than the
     * input, and returns that and the previous entry
     */
    [[nodiscard]] auto resistance_table_lookup(double resistance) const
        -> TableResult {
        auto compare = [resistance](auto elem) {
            return elem.first < resistance;
        };

        auto first_less =
            std::find_if(GetTable()().cbegin(), GetTable()().end(), compare);
        if (first_less == GetTable()().cbegin()) {
            return TableResult(TableError::TABLE_CBEGIN);
        }
        if (first_less == GetTable()().end()) {
            return TableResult(TableError::TABLE_END);
        }
        return TableResult(TableEntryPair(*first_less, *std::prev(first_less, 1)));
    }

    /**
     * Looks for the first table entry with a temperature LESS THAN than the
     * input, and returns that and the previous entry
     */
    [[nodiscard]] auto temperature_table_lookup(double temperature) const
        -> TableResult {
        auto compare = [temperature](auto elem) {
            return elem.second > temperature;
        };

        auto first_more =
            std::find_if(GetTable()().cbegin(), GetTable()().end(), compare);
        if (first_more == GetTable()().cbegin()) {
            return TableResult(TableError::TABLE_CBEGIN);
        }
        if (first_more == GetTable()().end()) {
            return TableResult(TableError::TABLE_END);
        }
        return TableResult(TableEntryPair(*first_more, *std::prev(first_more, 1)));
    }
};
};  // namespace thermistor_conversion
