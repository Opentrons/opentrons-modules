#pragma once

#include <concepts>
#include <numbers>

namespace lms {

struct BeltConfig {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float pulley_diameter;  // mm
    [[nodiscard]] constexpr auto get_mm_per_rev() const -> float {
        return static_cast<float>(pulley_diameter * std::numbers::pi);
    }
};

struct LeadScrewConfig {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float lead_screw_pitch;  // mm/rev
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float gear_reduction_ratio;  // large teeth / small teeth
    [[nodiscard]] constexpr auto get_mm_per_rev() const -> float {
        return lead_screw_pitch / gear_reduction_ratio;
    }
};

struct GearBoxConfig {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float gear_diameter;  // mm
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float gear_reduction_ratio;  // large teeth / small teeth
    [[nodiscard]] constexpr auto get_mm_per_rev() const -> float {
        return static_cast<float>((gear_diameter * std::numbers::pi) /
                                  gear_reduction_ratio);
    }
};

template <typename MC>
concept MotorMechanicalConfig = requires {
    std::is_same_v<MC, BeltConfig> || std::is_same_v<MC, LeadScrewConfig> ||
        std::is_same_v<MC, GearBoxConfig>;
};

template <MotorMechanicalConfig MEConfig>
struct LinearMotionSystemConfig {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MEConfig mech_config{};
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float steps_per_rev{};
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float microstep{};
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
