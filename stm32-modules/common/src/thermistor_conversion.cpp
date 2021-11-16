#include "core/thermistor_conversion.hpp"

#include <algorithm>
#include <cstdint>

#include "thermistor_lookups.hpp"

using namespace thermistor_conversion;

Conversion::Conversion(ThermistorType thermistor,
                       double bias_resistance_nominal_kohm,
                       uint8_t adc_max_bits)
    : _adc_max(static_cast<double>((1U << adc_max_bits) - 1)),
      _adc_max_result(
          static_cast<uint16_t>(static_cast<uint32_t>(1U << adc_max_bits) - 1)),
      _bias_resistance_kohm(bias_resistance_nominal_kohm),
      _type(thermistor) {}

Conversion::Conversion(ThermistorType thermistor,
                       double bias_resistance_nominal_kohm,
                       uint16_t adc_max_value, bool is_signed)
    : _adc_max(static_cast<double>(adc_max_value)),
      _adc_max_result(
          static_cast<uint16_t>(static_cast<uint32_t>(adc_max_value))),
      _bias_resistance_kohm(bias_resistance_nominal_kohm),
      _type(thermistor) {}

[[nodiscard]] auto Conversion::convert(uint16_t adc_count) const -> Result {
    auto resistance = resistance_from_adc(adc_count);
    if (std::holds_alternative<Error>(resistance)) {
        return resistance;
    }
    return temperature_from_resistance(std::get<double>(resistance));
}

[[nodiscard]] auto Conversion::resistance_from_adc(uint16_t adc_count) const
    -> Result {
    if (adc_count == _adc_max_result) {
        return Result(Error::OUT_OF_RANGE_LOW);
    }
    if (adc_count == 0) {
        return Result(Error::OUT_OF_RANGE_HIGH);
    }
    return Result(_bias_resistance_kohm /
                  ((_adc_max / static_cast<double>(adc_count)) - 1.0));
}

[[nodiscard]] auto Conversion::temperature_from_resistance(
    double resistance) const -> Result {
    auto entries = resistance_table_lookup(resistance);
    if (std::holds_alternative<TableError>(entries)) {
        if (std::get<TableError>(entries) == TableError::TABLE_END) {
            return Result(Error::OUT_OF_RANGE_HIGH);
        } else {
            return Result(Error::OUT_OF_RANGE_LOW);
        }
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

[[nodiscard]] auto Conversion::backconvert(double temperature) const
    -> uint16_t {
    auto entries = temperature_table_lookup(temperature);
    if (std::holds_alternative<TableError>(entries)) {
        if (std::get<TableError>(entries) == TableError::TABLE_END) {
            return _adc_max_result;
        } else {
            return 0;
        }
    }
    auto entry_pair = std::get<TableEntryPair>(entries);

    auto after_temp = static_cast<double>(entry_pair.first.second);
    auto after_res = entry_pair.first.first;
    auto before_temp = static_cast<double>(entry_pair.second.second);
    auto before_res = entry_pair.second.first;
    double resistance =
        ((after_res - before_res) / (after_temp - before_temp)) *
            (temperature - before_temp) +
        before_res;
    return static_cast<uint16_t>(_adc_max /
                                 ((_bias_resistance_kohm / resistance) + 1.0));
}

[[nodiscard]] auto Conversion::resistance_table_lookup(double resistance) const
    -> TableResult {
    auto compare = [resistance](auto elem) { return elem.first < resistance; };

    switch (_type) {
        case ThermistorType::NTCG104ED104DTDSX: {
            auto first_less =
                std::find_if(lookups::NTCG104ED104DTDSX().cbegin(),
                             lookups::NTCG104ED104DTDSX().end(), compare);
            if (first_less == lookups::NTCG104ED104DTDSX().cbegin()) {
                return TableResult(TableError::TABLE_CBEGIN);
            }
            if (first_less == lookups::NTCG104ED104DTDSX().end()) {
                return TableResult(TableError::TABLE_END);
            }
            return TableResult(TableEntryPair(*first_less, *(first_less - 1)));
        } break;
        case ThermistorType::KS103J2G: {
            auto first_less = std::find_if(lookups::KS103J2G().cbegin(),
                                           lookups::KS103J2G().end(), compare);
            if (first_less == lookups::KS103J2G().cbegin()) {
                return TableResult(TableError::TABLE_CBEGIN);
            }
            if (first_less == lookups::KS103J2G().end()) {
                return TableResult(TableError::TABLE_END);
            }
            return TableResult(TableEntryPair(*first_less, *(first_less - 1)));
        } break;
    }
    // Should never return here
    return TableResult(TableError::TABLE_END);
}

[[nodiscard]] auto Conversion::temperature_table_lookup(
    double temperature) const -> TableResult {
    auto compare = [temperature](auto elem) {
        return elem.second > temperature;
    };

    switch (_type) {
        case ThermistorType::NTCG104ED104DTDSX: {
            auto first_less =
                std::find_if(lookups::NTCG104ED104DTDSX().cbegin(),
                             lookups::NTCG104ED104DTDSX().end(), compare);
            if (first_less == lookups::NTCG104ED104DTDSX().cbegin()) {
                return TableResult(TableError::TABLE_CBEGIN);
            }
            if (first_less == lookups::NTCG104ED104DTDSX().end()) {
                return TableResult(TableError::TABLE_END);
            }
            return TableResult(TableEntryPair(*first_less, *(first_less - 1)));
        } break;
        case ThermistorType::KS103J2G: {
            auto first_less = std::find_if(lookups::KS103J2G().cbegin(),
                                           lookups::KS103J2G().end(), compare);
            if (first_less == lookups::KS103J2G().cbegin()) {
                return TableResult(TableError::TABLE_CBEGIN);
            }
            if (first_less == lookups::KS103J2G().end()) {
                return TableResult(TableError::TABLE_END);
            }
            return TableResult(TableEntryPair(*first_less, *(first_less - 1)));
        } break;
    }
    // Should never return here
    return TableResult(TableError::TABLE_END);
}
