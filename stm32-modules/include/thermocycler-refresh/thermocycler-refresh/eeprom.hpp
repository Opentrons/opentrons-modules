/**
 * @file eeprom.hpp
 * @brief Implements an EEPROm class that is specialized towards
 * holding the Thermal Offset Constants for the Thermocycler plate.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "core/at24c0xc.hpp"

namespace eeprom {

/**
 * @brief Constant values used for calculating the offset between
 * the physical thermistors on the system and the actual temperature on
 * the Thermocycler's plate.
 * @details
 * The temperature difference between the thermistors and the surface
 * of the thermocycler tends to scale with the magnitude of the thermistor
 * readings. Using two constants, B and C (for legacy purposes), the
 * resulting temperature relationship can be summarized as follows:
 *
 * > Plate Temp = ((B + 1) * Measured Temp) + C
 *
 * One of the EEPROM pages is reserved for a flag to indicate whether
 * the values have been written. The \ref EEPROMFlag enum captures the
 * valid states of this page. The page indicates what error detection,
 * if any, is included with the EEPROM constant values.
 *
 */
struct OffsetConstants {
    // The value of the constants B and C
    double b, c;
};

/**
 * @brief Encapsulates interactions with the EEPROM on the Thermocycler
 * mainboard. Allows reading and writing the thermal offset constants.
 */
template <size_t PAGES, uint8_t ADDRESS>
class Eeprom {
  public:
    Eeprom() : _eeprom() {}

    /**
     * @brief Get the offset constants from the EEPROM
     *
     * @tparam Policy for reading from EEPROM
     * @param policy Instance of Policy
     * @return OffsetConstants containing the B and C constants, or the
     * default values if the EEPROM doesn't have programmed values.
     */
    template <at24c0xc::AT24C0xC_Policy Policy>
    [[nodiscard]] auto get_offset_constants(Policy& policy) -> OffsetConstants {
        OffsetConstants ret = {.b = OFFSET_DEFAULT_CONST,
                               .c = OFFSET_DEFAULT_CONST};
        // Read the constantsss
        auto flag = read_const_flag(policy);

        if (flag == EEPROMFlag::WRITTEN_NO_CHECKSUM) {
            ret.b = read_const(EEPROMPageMap::CONST_B, policy);
            ret.c = read_const(EEPROMPageMap::CONST_C, policy);
        }
        _initialized = true;
        return ret;
    }

    /**
     * @brief Write new offset constants to the EEPROM
     *
     * @tparam Policy for writing to the EEPROM
     * @param constants OffsetConstants containing the B and C constants to
     * be written to EEPROM
     * @param policy Instance of Policy
     * @return True if the constants were written, false otherwise
     */
    template <at24c0xc::AT24C0xC_Policy Policy>
    auto write_offset_constants(OffsetConstants constants, Policy& policy)
        -> bool {
        // Write the constants
        if (!_eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_B), constants.b,
                policy)) {
            // Attempt to flag that the constants are not valid
            static_cast<void>(_eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_FLAG),
                static_cast<uint32_t>(EEPROMFlag::INVALID), policy));
            return false;
        }
        if (!_eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_C), constants.c,
                policy)) {
            // Attempt to flag that the constants are not valid
            static_cast<void>(_eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_FLAG),
                static_cast<uint32_t>(EEPROMFlag::INVALID), policy));
            return false;
        }
        // Flag that the constants are good
        return _eeprom.template write_value(
            static_cast<uint8_t>(EEPROMPageMap::CONST_FLAG),
            static_cast<uint32_t>(EEPROMFlag::WRITTEN_NO_CHECKSUM), policy);
    }

    /**
     * @brief Check if the EEPROM has been read since initialization.
     *
     * @return true if the eeprom has been read, false otherwise
     */
    [[nodiscard]] auto initialized() const -> bool { return _initialized; }

  private:
    // Enumeration of memory locations to be used on the EEPROM
    enum class EEPROMPageMap : uint8_t {
        CONST_B = 0,  // Value of the B constant
        CONST_C = 1,  // Value of the C constant
        // Flag indicating whether constants have been written.
        // Set to 1 to mark that constants are written with no checksum
        CONST_FLAG = 2
    };

    // Enumeration of the EEPROM_CONST_FLAG values
    enum class EEPROMFlag { WRITTEN_NO_CHECKSUM = 1, INVALID = 0xFF };

    static_assert(sizeof(EEPROMPageMap) == sizeof(uint8_t),
                  "EEPROM API requires uint8_t page address");

    static constexpr double OFFSET_DEFAULT_CONST = 0.0F;

    /**
     * @brief Read one of the constants on the device
     *
     * @tparam Policy class for reading from the eeprom
     * @param page Which page to read. Must be CONST_B or CONST_C
     * @param policy Instance of Policy for reading
     * @return double containing the constant
     */
    template <at24c0xc::AT24C0xC_Policy Policy>
    [[nodiscard]] auto read_const(EEPROMPageMap page, Policy& policy)
        -> double {
        if (page != EEPROMPageMap::CONST_FLAG) {
            auto val = _eeprom.template read_value<double>(
                static_cast<uint8_t>(page), policy);
            if (val.has_value()) {
                return val.value();
            }
        }
        return OFFSET_DEFAULT_CONST;
    }

    /**
     * @brief Read the Constants Flag in the EEPROM. This flag provides the
     * validity of the constants in memory.
     *
     * @tparam Policy class for reading from the eeprom
     * @param policy Instance of Policy for reading
     * @return EEPROMFlag containing the state of the constants in EEPROM
     */
    template <at24c0xc::AT24C0xC_Policy Policy>
    [[nodiscard]] auto read_const_flag(Policy& policy) -> EEPROMFlag {
        auto val = _eeprom.template read_value<uint32_t>(
            static_cast<uint8_t>(EEPROMPageMap::CONST_FLAG), policy);
        if (val.has_value()) {
            auto flag = val.value();
            if (flag ==
                static_cast<uint32_t>(EEPROMFlag::WRITTEN_NO_CHECKSUM)) {
                return EEPROMFlag::WRITTEN_NO_CHECKSUM;
            }
        }
        // Default
        return EEPROMFlag::INVALID;
    }

    // Handle for the actual EEPROM IC
    at24c0xc::AT24C0xC<PAGES, ADDRESS> _eeprom;
    // Whether the constants have been read from the EEPROM since startup.
    // Even if the EEPROM is empty, this flag is set after attempting
    // to read so that the firmware doesn't try to keep making redundant
    // reads.
    bool _initialized = false;
};

}  // namespace eeprom
