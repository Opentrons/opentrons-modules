#pragma once

#include <concepts>
#include <numbers>

namespace lms {

class BeltConfig {
    float mm_per_rev(float pulley_diameter) {
        return static_cast<float>(pulley_diameter * std::numbers::pi);
    }
};

class LeadScrewConfig {
    float mm_per_rev(float lead_screw_pitch, float gear_reduction_ratio) {
        return lead_screw_pitch / gear_reduction_ratio;
    }
};

class GearBoxConfig {
    float mm_per_rev(float gear_diameter, float gear_reduction_ratio) {
        return static_cast<float>((gear_diameter * std::numbers::pi) /
                                  gear_reduction_ratio);
    }
};


struct LinearMotionSystemConfig {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float mm_per_rev;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float steps_per_rev;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float microstep;
    [[nodiscard]] constexpr auto get_usteps_per_mm() const -> float {
        return (steps_per_rev * microstep) / (mech_config.get_mm_per_rev());
    }
    [[nodiscard]] constexpr auto get_usteps_per_um() const -> float {
        return (steps_per_rev * microstep) /
               // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
               (mech_config.get_mm_per_rev() * 1000.0);
    }
    [[nodiscard]] constexpr auto get_um_per_step() const -> float {
        return (mech_config.get_mm_per_rev()) / (steps_per_rev * microstep) *
               // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
               1000;
    }
};

}  // namespace lms
