/**
 * @file eeprom.hpp
 * @brief Implements an EEPROM class that is specialized towards
 * holding the Thermal Offset Constants for the Temperature Deck plate.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "core/m24128.hpp"

namespace eeprom {

/**
 * @brief Constant values used for calculating the offset between
 * the physical thermistors on the system and the actual temperature on
 * the Temperature Deck's plate.
 * @details
 * The temperature difference between the thermistors and the surface
 * of the thermocycler tends to scale with the magnitude of the thermistor
 * readings. Using three constants, A B and C, the equation is:
 *
 * > Plate Temp = A * (heatsink temp) + ((B + 1) * Measured Temp) + C
 *
 * One of the EEPROM pages is reserved for a flag to indicate whether
 * the values have been written. The \ref EEPROMFlag enum captures the
 * valid states of this page. The page indicates what error detection,
 * if any, is included with the EEPROM constant values.
 *
 */
struct __attribute__((packed)) OffsetConstants {
    // Constant A is the same for each channel
    double a, b, c;
};

struct __attribute__((packed)) PageContent {
    uint8_t constant_flag;
    OffsetConstants constants;
};

/**
 * @brief Encapsulates interactions with the EEPROM on the Thermocycler
 * mainboard. Allows reading and writing the thermal offset constants.
 */
template <uint8_t ADDRESS>
class Eeprom {
  public:
    Eeprom() : _eeprom() {}

    /**
     * @brief Get the offset constants from the EEPROM
     *
     * @tparam Policy for reading from EEPROM
     * @param defaults OffsetConstants containing default values to return
     *                 in the case that the EEPROM is not written.
     * @param policy Instance of Policy
     * @return OffsetConstants containing the A, B and C constants, or the
     * default values if the EEPROM doesn't have programmed values.
     */
    template <m24128::M24128_Policy Policy>
    [[nodiscard]] auto get_offset_constants(const OffsetConstants& defaults,
                                            Policy& policy) -> OffsetConstants {
        OffsetConstants ret = defaults;
        // Read the constants
        auto readback = _eeprom.template read_value<PageContent>(
            static_cast<uint8_t>(EEPROMPageMap::CONSTANTS), policy);

        if (readback.has_value()) {
            auto& values = readback.value();
            if (values.constant_flag ==
                static_cast<uint8_t>(EEPROMFlag::CONSTANTS_WRITTEN)) {
                ret.a = values.constants.a;
                ret.b = values.constants.b;
                ret.c = values.constants.c;
            }
        }

        return ret;
    }

    /**
     * @brief Write new offset constants to the EEPROM
     *
     * @tparam Policy for writing to the EEPROM
     * @param constants OffsetConstants containing the constants to
     * be written to EEPROM
     * @param policy Instance of Policy
     * @return True if the constants were written, false otherwise
     */
    template <m24128::M24128_Policy Policy>
    auto write_offset_constants(OffsetConstants constants, Policy& policy)
        -> bool {
        // Write the constants
        auto to_write = PageContent{.constant_flag = static_cast<uint8_t>(
                                        EEPROMFlag::CONSTANTS_WRITTEN),
                                    .constants = constants};
        auto ret = _eeprom.template write_value(
            static_cast<uint8_t>(EEPROMPageMap::CONSTANTS), to_write, policy);
        if (!ret) {
            // Attempt to flag that the constants are not valid
            static_cast<void>(_eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONSTANTS),
                static_cast<uint8_t>(EEPROMFlag::INVALID), policy));
        }
        return ret;
    }

    /**
     * @brief Check if the EEPROM has been read since initialization.
     *
     * @return true if the eeprom has been read, false otherwise
     */
    [[nodiscard]] auto initialized() const -> bool { return _initialized; }

  private:
    // Enumeration of memory locations to be used on the EEPROM
    enum class EEPROMPageMap : uint8_t { CONSTANTS };

    // Enumeration of the EEPROM_CONST_FLAG values
    enum class EEPROMFlag : uint8_t {
        CONSTANTS_WRITTEN = 1,  // Values of all constants are written
        INVALID = 0xFF          // No values are written
    };

    static_assert(sizeof(EEPROMPageMap) == sizeof(uint8_t),
                  "EEPROM API requires uint8_t page address");

    /** Default value for all constants.*/
    static constexpr double OFFSET_DEFAULT_CONST = 0.0F;

    // Handle for the actual EEPROM IC
    m24128::M24128<ADDRESS> _eeprom;
    // Whether the constants have been read from the EEPROM since startup.
    // Even if the EEPROM is empty, this flag is set after attempting
    // to read so that the firmware doesn't try to keep making redundant
    // reads.
    bool _initialized = false;
};

}  // namespace eeprom
