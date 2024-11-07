#pragma once

#include <concepts>
#include <numbers>

namespace lms {

struct BeltConfig {
    static constexpr auto mm_per_rev(float pulley_diameter) -> float {
        return static_cast<float>(pulley_diameter * std::numbers::pi);
    }
};

struct LeadScrewConfig {
    static constexpr auto mm_per_rev(float lead_screw_pitch,
                                     float gear_reduction_ratio) -> float {
        return lead_screw_pitch / gear_reduction_ratio;
    }
};

struct GearBoxConfig {
    static constexpr auto mm_per_rev(float gear_diameter,
                                     float gear_reduction_ratio) -> float {
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
        return (steps_per_rev * microstep) / mm_per_rev;
    }
};

}  // namespace lms
