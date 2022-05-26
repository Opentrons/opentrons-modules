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
 * > Plate Temp = A * (heatsink temp) + ((B + 1) * Measured Temp) + C
 *
 * One of the EEPROM pages is reserved for a flag to indicate whether
 * the values have been written. The \ref EEPROMFlag enum captures the
 * valid states of this page. The page indicates what error detection,
 * if any, is included with the EEPROM constant values.
 *
 */
struct OffsetConstants {
    // Constant A is the same for each channel
    double a;
    double bl, cl;  // B and C for left
    double bc, cc;  // B and C for center
    double br, cr;  // B and C for right
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
     * @param defaults OffsetConstants containing default values to return
     *                 in the case that the EEPROM is not written.
     * @param policy Instance of Policy
     * @return OffsetConstants containing the A, B and C constants, or the
     * default values if the EEPROM doesn't have programmed values.
     */
    template <at24c0xc::AT24C0xC_Policy Policy>
    [[nodiscard]] auto get_offset_constants(const OffsetConstants& defaults,
                                            Policy& policy) -> OffsetConstants {
        OffsetConstants ret = defaults;
        // Read the constantsss
        auto flag = read_const_flag(policy);

        if (flag == EEPROMFlag::CONSTANTS_WRITTEN) {
            ret.a = read_const(EEPROMPageMap::CONST_A, policy);
            ret.bl = read_const(EEPROMPageMap::CONST_BL, policy);
            ret.cl = read_const(EEPROMPageMap::CONST_CL, policy);
            ret.bc = read_const(EEPROMPageMap::CONST_BC, policy);
            ret.cc = read_const(EEPROMPageMap::CONST_CC, policy);
            ret.br = read_const(EEPROMPageMap::CONST_BR, policy);
            ret.cr = read_const(EEPROMPageMap::CONST_CR, policy);
        }
        _initialized = true;
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
    template <at24c0xc::AT24C0xC_Policy Policy>
    auto write_offset_constants(OffsetConstants constants, Policy& policy)
        -> bool {
        // Write the constants
        auto ret = _eeprom.template write_value(
            static_cast<uint8_t>(EEPROMPageMap::CONST_A), constants.a, policy);
        if (ret) {
            ret = _eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_BL), constants.bl,
                policy);
        }
        if (ret) {
            ret = _eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_CL), constants.cl,
                policy);
        }
        if (ret) {
            ret = _eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_BC), constants.bc,
                policy);
        }
        if (ret) {
            ret = _eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_CC), constants.cc,
                policy);
        }
        if (ret) {
            ret = _eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_BR), constants.br,
                policy);
        }
        if (ret) {
            ret = _eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_CR), constants.cr,
                policy);
        }
        if (ret) {
            // Flag that the constants are good
            ret = _eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_FLAG),
                static_cast<uint32_t>(EEPROMFlag::CONSTANTS_WRITTEN), policy);
        }
        if (!ret) {
            // Attempt to flag that the constants are not valid
            static_cast<void>(_eeprom.template write_value(
                static_cast<uint8_t>(EEPROMPageMap::CONST_FLAG),
                static_cast<uint32_t>(EEPROMFlag::INVALID), policy));
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
    enum class EEPROMPageMap : uint8_t {
        CONST_BL = 0,  // Value of the B constant for the left channel
        CONST_CL = 1,  // Value of the C constant for the left channel
        // Flag indicating whether constants have been written.
        // See \ref EEPROMFlag
        CONST_FLAG = 2,
        CONST_A = 3,   // Value of the A constant
        CONST_BC = 4,  // Value of the B constant for the center channel
        CONST_CC = 5,  // Value of the C constant for the center channel
        CONST_BR = 6,  // Value of the B constant for the right channel
        CONST_CR = 7,  // Value of the C constant for the right channel
    };

    // Enumeration of the EEPROM_CONST_FLAG values
    enum class EEPROMFlag {
        CONSTANTS_WRITTEN = 3,  // Values of all constants are written (7 total)
        INVALID = 0xFF          // No values are written
    };

    static_assert(sizeof(EEPROMPageMap) == sizeof(uint8_t),
                  "EEPROM API requires uint8_t page address");

    /** Default value for all constants.*/
    static constexpr double OFFSET_DEFAULT_CONST = 0.0F;

    /**
     * @brief Read one of the constants on the device
     *
     * @tparam Policy class for reading from the eeprom
     * @param page Which page to read. Must be a valid page
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
            if (flag == static_cast<uint32_t>(EEPROMFlag::CONSTANTS_WRITTEN)) {
                return EEPROMFlag::CONSTANTS_WRITTEN;
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
