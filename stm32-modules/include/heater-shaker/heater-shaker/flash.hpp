/**
 * @file flash.hpp
 * @brief Implements a FLASH class that is specialized towards
 * holding the Thermal Offset Constants for the Heater-Shaker heat plate.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace flash {

/**
 * @brief Constant values used for calculating the offset between
 * the physical thermistors on the system and the actual temperature on
 * the Heater-Shaker's heat plate.
 * @details
 * The temperature difference between the thermistors and the surface
 * of the Heater-Shaker tends to scale with the magnitude of the thermistor
 * readings. Using two constants, B and C (for legacy purposes), the
 * resulting temperature relationship can be summarized as follows:
 *
 * > Plate Temp = ((B + 1) * Measured Temp) + C
 *
 * One of the FLASH addresses is reserved for a flag to indicate whether
 * the values have been written. The \ref FLASHFlag enum captures the
 * valid states of this page. The page indicates what error detection,
 * if any, is included with the FLASH constant values.
 *
 */
struct OffsetConstants {
    // The value of the constants B and C
    double b, c;
    bool flag;
};

/**
 * @brief Encapsulates interactions with the FLASH on the Heater-Shaker
 * mainboard. Allows reading and writing the thermal offset constants.
 */
class Flash {
  public:
    Flash() {}

    /**
     * @brief Get the offset constants from FLASH
     *
     * @tparam Policy for reading from FLASH
     * @param policy Instance of Policy
     * @return OffsetConstants containing the B and C constants, or the
     * default values if the FLASH doesn't have programmed values.
     */
    template <typename Policy>
    [[nodiscard]] auto get_offset_constants(Policy& policy)
        -> OffsetConstants {
        OffsetConstants ret = {.b = OFFSET_DEFAULT_CONST,
                               .c = OFFSET_DEFAULT_CONST};
        OffsetConstants receive = policy.get_thermal_offsets();
        if (receive.flag == static_cast<bool>(FLASHFlag::WRITTEN_NO_CHECKSUM)) {
            ret.b = receive.b;
            ret.c = receive.c;
        }
        _initialized = true;
        return ret;
    }

    /**
     * @brief Write new offset constants to the FLASH
     *
     * @tparam Policy for writing to the FLASH
     * @param constants OffsetConstants containing the B and C constants to
     * be written to FLASH
     * @param policy Instance of Policy
     * @return True if the constants were written, false otherwise
     */
    template <typename Policy>
    auto set_offset_constants(OffsetConstants constants, Policy& policy)
        -> bool {
        if (!policy.set_thermal_offsets(&constants)) {
            return false;
        } else {
            _initialized = true;
            return true;
        }
    }

    /**
     * @brief Check if the FLASH has been read since initialization.
     *
     * @return true if the flash has been read, false otherwise
     */
    [[nodiscard]] auto initialized() const -> bool { return _initialized; }

    // Enumeration of the FLASH_CONST_FLAG values
    enum class FLASHFlag : uint64_t { WRITTEN_NO_CHECKSUM = 1, INVALID = 0 };

  private:
    /** Default value for all constants.*/
    static constexpr double OFFSET_DEFAULT_CONST = 0.0F;

    // Whether the constants have been read from the FLASH since startup.
    // Even if the FLASH is empty, this flag is set after attempting
    // to read so that the firmware doesn't try to keep making redundant
    // reads.
    bool _initialized = false;
};

}  // namespace flash
