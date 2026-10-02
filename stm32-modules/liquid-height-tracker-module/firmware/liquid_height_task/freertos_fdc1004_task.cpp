#include "firmware/freertos_fdc1004_task.hpp"

#include <iostream>

#include "FreeRTOS.h"
#include "liquid-height-tracker-module/fdc1004.hpp"
#include "task.h"

namespace fdc1004::tasks {

static void RunLiquidHeightTrackingLoop(void* argument);
static constexpr uint16_t CONFIG = 10;
static constexpr uint16_t STACK_SIZE = 512;

auto FDC1004_Task_Register(i2c::hardware::I2C* i2c_bus_handle) -> void {
    xTaskCreate(RunLiquidHeightTrackingLoop, "FDC1004Task", STACK_SIZE,
                static_cast<void*>(i2c_bus_handle), 2, nullptr);
}

static void RunLiquidHeightTrackingLoop(void* argument) {
    auto* i2c_bus = static_cast<i2c::hardware::I2C*>(argument);

    if (i2c_bus == nullptr) {
        std::cerr
            << "CRITICAL: FDC1004 Task passed a null I2C bus driver pointer!"
            << std::endl;
        vTaskSuspend(nullptr);
        return;
    }

    fdc1004::FDC1004 capacitive_sensor{i2c_bus};

    if (!capacitive_sensor.who_am_i() || !capacitive_sensor.reset()) {
        std::cerr << "FDC1004 Sensor Interface Communications Initial "
                     "Handshake Failed!"
                  << std::endl;
        vTaskSuspend(nullptr);
        return;
    }

    if (!capacitive_sensor.configure_single_ended(0, CONFIG)) {
        std::cerr << "FDC1004 Single Ended channel configuration failed!"
                  << std::endl;
        vTaskSuspend(nullptr);
        return;
    }

    double measured_capacitance_pf{0.0};

    for (;;) {
        if (capacitive_sensor.trigger_measurement(
                0, fdc1004::FDC1004::Rate::SAMPLES_100HZ)) {
            while (!capacitive_sensor.is_measurement_done(0)) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if (capacitive_sensor.read_measurement(0,
                                                   measured_capacitance_pf)) {
                std::cout << "Measured Level: " << measured_capacitance_pf
                          << " pF" << std::endl;
            } else {
                std::cerr
                    << "Failed parsing data from measurement register blocks."
                    << std::endl;
            }
        } else {
            std::cerr << "Failed to trigger FDC1004 measurement loop iteration."
                      << std::endl;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

}  // namespace fdc1004::tasks
