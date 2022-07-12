#pragma once

#include <array>

#include "firmware/system_hardware.h"
#include "firmware/system_serial_number.h"
#include "systemwide.h"
#include "tempdeck-gen3/errors.hpp"

class SystemPolicy {
  private:
    static constexpr std::size_t SYSTEM_SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    static constexpr uint8_t ADDRESS_LENGTH = 8;
    static constexpr uint8_t ADDRESSES =
        SYSTEM_SERIAL_NUMBER_LENGTH / ADDRESS_LENGTH;

  public:
    auto enter_bootloader() -> void;
    auto set_serial_number(
        std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number)
        -> errors::ErrorCode;
    auto get_serial_number() -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH>;
};
