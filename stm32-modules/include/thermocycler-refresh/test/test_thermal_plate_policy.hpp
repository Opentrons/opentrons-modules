#pragma once

class TestThermalPlatePolicy {
    private:
        bool _enabled = false;
    public:
        auto set_enabled(bool enabled) -> void {
            _enabled = enabled;
        }
};
