#include "firmware/freertos_fdc1004_task.hpp"

#include <variant>

#include "FreeRTOS.h"
#include "firmware/fdc1004_hardware.h"
#include "firmware/freertos_message_queue.hpp"
#include "incubator-module/fdc1004.hpp"
#include "incubator-module/messages.hpp"
#include "task.h"

namespace fdc1004::tasks {

static constexpr uint16_t CONFIG = 10;
static constexpr uint32_t MEASUREMENT_TIMEOUT_MS = 250;

using CapacitiveQueue = ::tasks::FirmwareTasks::CapacitiveQueue;
using QueueAggregator = ::tasks::FirmwareTasks::QueueAggregator;

static CapacitiveQueue capacitive_queue{1, "Capacitive Queue"};

static auto measure_channel(fdc1004::FDC1004& sensor, uint8_t channel,
                            double& capacitance_pf) -> bool {
    if (!sensor.trigger_measurement(channel,
                                    fdc1004::FDC1004::Rate::SAMPLES_400HZ)) {
        return false;
    }

    const auto start_time = xTaskGetTickCount();
    const auto timeout = pdMS_TO_TICKS(MEASUREMENT_TIMEOUT_MS);
    while (!sensor.is_measurement_done(channel)) {
        if ((xTaskGetTickCount() - start_time) >= timeout) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return sensor.read_measurement(channel, capacitance_pf);
}

static auto read_all_channels(fdc1004::FDC1004& sensor, bool sensor_initialized,
                              double& channel_1, double& channel_2,
                              double& channel_3, double& channel_4) -> bool {
    return sensor_initialized && measure_channel(sensor, 0, channel_1) &&
           measure_channel(sensor, 1, channel_2) &&
           measure_channel(sensor, 2, channel_3) &&
           measure_channel(sensor, 3, channel_4);
}

static auto handle_state_request(
    const messages::GetCapacitiveStateMessage& request,
    fdc1004::FDC1004& sensor, QueueAggregator* aggregator,
    bool sensor_initialized) -> void {
    double channel_1 = 0.0;
    double channel_2 = 0.0;
    double channel_3 = 0.0;
    double channel_4 = 0.0;
    const bool reading_failed = !read_all_channels(
        sensor, sensor_initialized, channel_1, channel_2, channel_3, channel_4);
    const auto response = messages::GetCapacitiveStateResponseMessage{
        .responding_to_id = request.id,
        .with_error = reading_failed,
        .capacitive_ch1 = channel_1,
        .capacitive_ch2 = channel_2,
        .capacitive_ch3 = channel_3,
        .capacitive_ch4 = channel_4,
    };
    static_cast<void>(aggregator->send_to_address(
        response, ::tasks::FirmwareTasks::HostCommsAddress, pdMS_TO_TICKS(10)));
}

auto run(QueueAggregator* aggregator, i2c::hardware::I2C* i2c_bus_handle)
    -> void {
    if (aggregator == nullptr || i2c_bus_handle == nullptr) {
        vTaskSuspend(nullptr);
        return;
    }

    fdc1004::FDC1004 capacitive_sensor{i2c_bus_handle};

    capacitive_queue.provide_handle(xTaskGetCurrentTaskHandle());
    if (!aggregator->register_queue(capacitive_queue)) {
        vTaskSuspend(nullptr);
        return;
    }
    capacitive_queue.set_ready();

    const bool sensor_initialized =
        fdc1004_hardware_init(I2C_BUS_1, 0, CONFIG) &&
        capacitive_sensor.configure_single_ended(1, CONFIG) &&
        capacitive_sensor.configure_single_ended(2, CONFIG) &&
        capacitive_sensor.configure_single_ended(3, CONFIG);
    messages::CapacitiveMessage message{};
    for (;;) {
        if (capacitive_queue.try_recv(&message)) {
            if (const auto* request =
                    std::get_if<messages::GetCapacitiveStateMessage>(
                        &message)) {
                handle_state_request(*request, capacitive_sensor, aggregator,
                                     sensor_initialized);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

}  // namespace fdc1004::tasks
