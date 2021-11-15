/**
 * The LidHeaterPolicy class provides firmware implementation
 * of any stubbable hardware interactions needed in the Thermal
 * Plate Task.
 */
#pragma once

class LidHeaterPolicy {
  public:
    LidHeaterPolicy();

    auto set_enabled(bool enabled) -> void;
};
