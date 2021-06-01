#include "heater-shaker/thermistor_conversion.hpp"

#include <algorithm>
#include <cstdint>

#include "thermistor_lookups.hpp"

using namespace thermistor_conversion;

Conversion::Conversion(ThermistorType thermistor,
                       double bias_resistance_nominal_kohm,
                       uint8_t adc_max_bits)
    : adc_max(static_cast<double>((1U << adc_max_bits) - 1)),
      adc_max_result(
          static_cast<uint16_t>(static_cast<uint32_t>(1U << adc_max_bits) - 1)),
      bias_resistance_kohm(bias_resistance_nominal_kohm) {}

[[nodiscard]] auto Conversion::convert(uint16_t adc_count) const -> Result {
    auto resistance = resistance_from_adc(adc_count);
    if (std::holds_alternative<Error>(resistance)) {
        return resistance;
    }
    return temperature_from_resistance(std::get<double>(resistance));
}

[[nodiscard]] auto Conversion::resistance_from_adc(uint16_t adc_count) const
    -> Result {
    if (adc_count == adc_max_result) {
        return Result(Error::OUT_OF_RANGE_LOW);
    }
    if (adc_count == 0) {
        return Result(Error::OUT_OF_RANGE_HIGH);
    }
    return Result(bias_resistance_kohm /
                  ((adc_max / static_cast<double>(adc_count)) - 1.0));
}

[[nodiscard]] auto Conversion::temperature_from_resistance(
    double resistance) const -> Result {
    auto first_less = std::find_if(
        lookups::NTCG104ED104DTDSX().cbegin(),
        lookups::NTCG104ED104DTDSX().end(),
        [resistance](auto elem) { return elem.first < resistance; });
    if (first_less == lookups::NTCG104ED104DTDSX().cend()) {
        return Result(Error::OUT_OF_RANGE_HIGH);
    }
    if (first_less == lookups::NTCG104ED104DTDSX().cbegin()) {
        return Result(Error::OUT_OF_RANGE_LOW);
    }
    auto after_temp = static_cast<double>(first_less->second);
    auto after_res = first_less->first;
    auto before = first_less - 1;
    auto before_temp = static_cast<double>(before->second);
    auto before_res = before->first;

    return Result((after_temp - before_temp) / (after_res - before_res) *
                      (resistance - before_res) +
                  before_temp);
}

[[nodiscard]] auto Conversion::backconvert(double temperature) const
    -> uint16_t {
    auto first_greater = std::find_if(
        lookups::NTCG104ED104DTDSX().cbegin(),
        lookups::NTCG104ED104DTDSX().end(),
        [temperature](auto elem) { return elem.second > temperature; });

    if (first_greater == lookups::NTCG104ED104DTDSX().end()) {
        return adc_max_result;
    }
    if (first_greater == lookups::NTCG104ED104DTDSX().cbegin()) {
        return 0;
    }

    auto after_temp = static_cast<double>(first_greater->second);
    auto after_res = static_cast<double>(first_greater->first);
    auto before = first_greater - 1;
    auto before_temp = static_cast<double>(before->second);
    auto before_res = static_cast<double>(before->first);
    double resistance =
        ((after_res - before_res) / (after_temp - before_temp)) *
            (temperature - before_temp) +
        before_res;
    return static_cast<uint16_t>(adc_max /
                                 ((bias_resistance_kohm / resistance) + 1.0));
}
