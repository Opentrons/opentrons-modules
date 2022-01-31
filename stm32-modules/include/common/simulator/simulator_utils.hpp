/**
 * @file simulator_utils.hpp
 * @brief common utilities for simulator applications
 */

#pragma once

#include <array>
#include <cstdlib>
#include <optional>

namespace simulator_utils {

/**
 * @brief Get the serial number from an environment variable
 *
 * @tparam N Number of character bytes in thhe serial number array
 * @param[in] var_name The name of the environment variable to read
 * @return The serial number \e if one is defined in the environment variable
 * \c var_name
 */
template <size_t N>
static auto get_serial_number(const char *var_name)
    -> std::optional<std::array<char, N>> {
    using RT = std::optional<std::array<char, N>>;
    if (var_name == NULL) {
        return RT();
    }

    auto *env_p = std::getenv(var_name);
    if (!env_p || (strlen(env_p) == 0)) {
        return RT();
    }

    std::array<char, N> ret;
    memcpy(ret.data(), env_p, std::min(strlen(env_p), N));
    return RT(ret);
}

}  // namespace simulator_utils
