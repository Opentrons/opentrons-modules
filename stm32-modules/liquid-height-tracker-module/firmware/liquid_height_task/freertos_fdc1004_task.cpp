// #include "freertos_fdc1004_task.hpp"
// #pragma once
#include "FreeRTOS.h"
#include "task.h"
#include "firmware/freertos_fdc1004_task.hpp"
#include "liquid-height-tracker-module/fdc1004.hpp"
#include "firmware/i2c_comms.hpp"
#include <iostream>

namespace fdc1004::tasks {

// Cached pointer reference tracking our injected I2C physical hardware bus line
static i2c::hardware::I2C* global_i2c_bus = nullptr;

// Forward declaration of the native C-style thread worker function execution block
static void RunLiquidHeightTrackingLoop(void* argument);

auto FDC1004_Task_Register(i2c::hardware::I2C* i2c_bus_handle) -> void {
    global_i2c_bus = i2c_bus_handle;

    xTaskCreate(
        RunLiquidHeightTrackingLoop,         
        "FDC1004Task",                       
        512,                                 
        nullptr,                             
        2,                                   
        nullptr                              
    );
}

static void RunLiquidHeightTrackingLoop(void* argument) {
    (void)argument; 

    if (global_i2c_bus == nullptr) {
        std::cerr << "CRITICAL: FDC1004 Task passed a null I2C bus driver pointer!" << std::endl;
        vTaskSuspend(nullptr);
    }

    // Force explicit conversion to match the exact constructor signature 
    // expected by your fdc1004.hpp / fdc1004.cpp binary definitions (I2CBase*)
    auto capacitySensor = fdc1004::FDC1004(static_cast<i2c::hardware::I2CBase*>(global_i2c_bus));

    if (!capacitySensor.who_am_i() || !capacitySensor.reset()) {
        std::cerr << "FDC1004 Sensor Interface Communications Initial Handshake Failed!" << std::endl;
        vTaskSuspend(nullptr);
    }

    if (!capacitySensor.configure_single_ended(0, 10)) {
        std::cerr << "FDC1004 Single Ended channel configuration failed!" << std::endl;
        vTaskSuspend(nullptr);
    }

    double measured_capacitance_pf = 0.0;
    
    for (;;) {
        if (capacitySensor.trigger_measurement(0, fdc1004::FDC1004::Rate::SAMPLES_100HZ)) {
            
            while (!capacitySensor.is_measurement_done(0)) {
                vTaskDelay(pdMS_TO_TICKS(10)); 
            }

            if (capacitySensor.read_measurement(0, measured_capacitance_pf)) {
                std::cout << "Measured Level: " << measured_capacitance_pf << " pF" << std::endl;
            } else {
                std::cerr << "Failed parsing data from measurement register blocks." << std::endl;
            }
        } else {
            std::cerr << "Failed to trigger FDC1004 measurement loop iteration." << std::endl;
        }

        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}

} // namespace fdc1004::tasks