#include "core/version.hpp"

static constexpr const char* _FW_VERSION_GENERATED = "(test-dev)";
static constexpr const char* _HW_VERSION_GENERATED =
    "Opentrons-liquid-height-tracker-module-b2";

const char* version::fw_version() { return _FW_VERSION_GENERATED; }

const char* version::hw_version() { return _HW_VERSION_GENERATED; }
