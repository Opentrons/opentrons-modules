#include <array>
#include <cmath>

#include "catch2/catch.hpp"
#include "firmware/hardware_iface.hpp"
#include "systemwide.h"
#include "vacuum-module/mprll0025pa00001a.hpp"

namespace vacuum_pressure_sensor {

using RxTxReturn = uint8_t;

struct MockPolicy {
    uint32_t press_counts = 0;
    uint8_t status = 0;

    void sleep_ms(uint32_t) {}

    bool is_device_ready(uint16_t) { return true; }

    RxTxReturn i2c_master_write(uint16_t, const uint8_t *, uint16_t) {
        return 0;
    }

    RxTxReturn i2c_master_read(uint16_t, uint8_t *data, uint16_t len) {
        if (len == 4) {
            data[0] = status;
            data[1] = (press_counts >> 16) & 0xFF;
            data[2] = (press_counts >> 8) & 0xFF;
            data[3] = press_counts & 0xFF;
        }
        return 0;
    }

    RxTxReturn i2c_read(uint16_t dev_addr, uint16_t reg, uint8_t *data,
                        uint16_t size) {
        return 0;
    }

    RxTxReturn i2c_write(uint16_t dev_addr, uint16_t reg, uint8_t *data,
                         uint16_t size) {
        return 0;
    }
};

auto get_counts(double mbar) -> uint32_t {
    double psi = mbar / PSI2MBAR;
    double counts = psi * (OUTPUT_MAX - OUTPUT_MIN) / PMAX + OUTPUT_MIN;
    return static_cast<uint32_t>(std::round(counts));
}

}  // namespace vacuum_pressure_sensor

using namespace vacuum_pressure_sensor;

SCENARIO("MPRLL0025PA00001 pressure filtering works", "[vacuum][filter]") {
    GIVEN("a mock policy and initialized sensor") {
        MockPolicy policy;
        MPRLL0025PA00001<MockPolicy> sensor(DEV_ADDRESS);
        sensor.initialize(&policy, PressureSensorID::ABS_PRESSURE_A);
        policy.status = 0;

        WHEN("first read with pressure 10.0") {
            policy.press_counts = get_counts(10.0);
            auto result = sensor.read_pressure();
            THEN("returns approximately 10.0") {
                REQUIRE_THAT(result, Catch::Matchers::WithinAbs(10.0, 0.01));
            }
        }

        GIVEN("after first read with constant pressure 10.0") {
            policy.press_counts = get_counts(10.0);
            sensor.read_pressure();

            WHEN("multiple subsequent reads with same pressure") {
                bool all_constant = true;
                for (int i = 0; i < 10; ++i) {
                    auto result = sensor.read_pressure();
                    if (!Catch::Matchers::Floating::WithinAbsMatcher(10.0, 0.01)
                             .match(result)) {
                        all_constant = false;
                        break;
                    }
                }
                THEN("all return approximately 10.0") { REQUIRE(all_constant); }
            }
        }

        GIVEN("after first read with 10.0") {
            policy.press_counts = get_counts(10.0);
            sensor.read_pressure();

            WHEN("second read with 20.0") {
                policy.press_counts = get_counts(20.0);
                auto result = sensor.read_pressure();
                THEN("returns approximately 12.405") {
                    REQUIRE_THAT(result,
                                 Catch::Matchers::WithinAbs(12.405, 0.001));
                }

                AND_WHEN("third read with 30.0") {
                    policy.press_counts = get_counts(30.0);
                    auto result = sensor.read_pressure();
                    THEN("returns approximately 16.731") {
                        REQUIRE_THAT(result,
                                     Catch::Matchers::WithinAbs(16.731, 0.001));
                    }
                }
            }
        }

        GIVEN("after sequence of reads to fill and wrap the buffer") {
            std::array<double, 9> inputs = {10.0, 20.0, 30.0, 40.0, 50.0,
                                            60.0, 70.0, 80.0, 90.0};
            std::array<double, 9> expected = {10.0,   12.405, 16.731,
                                              22.595, 29.69,  37.77,
                                              46.638, 56.136, 66.138};

            bool all_match = true;
            for (size_t i = 0; i < inputs.size(); ++i) {
                policy.press_counts = get_counts(inputs[i]);
                auto result = sensor.read_pressure();
                if (!Catch::Matchers::Floating::WithinAbsMatcher(expected[i],
                                                                 0.003)
                         .match(result)) {
                    all_match = false;
                    break;
                }
            }
            THEN("filtered values match expected after wrap-around") {
                REQUIRE(all_match);
            }
        }
    }
}
