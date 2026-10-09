#include <array>
#include <cstddef>
#include <cstdio>
#include <variant>

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/proximity_sensor.h"
#include "firmware/serial_hardware.h"
#include "incubator-module/messages.hpp"
#include "task.h"

namespace {

auto sensor_name(proximity_sensor_id_t id) -> const char* {
    switch (id) {
        case PROXIMITY_SENSOR_PC2:
            return "PC2";
        default:
            return "unknown";
    }
}

}  // namespace

namespace proximity_control_task {

using ProximityQueue = ::tasks::FirmwareTasks::ProximityQueue;
using QueueAggregator = ::tasks::FirmwareTasks::QueueAggregator;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static ProximityQueue proximity_queue{1, "Proximity Queue"};

static auto handle_state_request(
    const messages::GetProximityStateMessage& request,
    QueueAggregator* aggregator) -> void {
    const auto response = messages::GetProximityStateResponseMessage{
        .responding_to_id = request.id,
        .pc2_detected = proximity_sensor_read_raw(PROXIMITY_SENSOR_PC2),
    };
    static_cast<void>(aggregator->send_to_address(
        response, ::tasks::FirmwareTasks::HostCommsAddress, pdMS_TO_TICKS(10)));
}

auto run(QueueAggregator* aggregator) -> void {
    proximity_sensor_attach_task(xTaskGetCurrentTaskHandle());
    if (aggregator != nullptr) {
        proximity_queue.provide_handle(xTaskGetCurrentTaskHandle());
        if (aggregator->register_queue(proximity_queue)) {
            proximity_queue.set_ready();
        }
    }

    messages::ProximityMessage message{};
    while (true) {
        static_cast<void>(
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(PROXIMITY_SAMPLE_MS)));
        proximity_sensor_poll();
        while (aggregator != nullptr && proximity_queue.try_recv(&message)) {
            if (const auto* request =
                    std::get_if<messages::GetProximityStateMessage>(&message)) {
                handle_state_request(*request, aggregator);
            }
        }
    }
}

}  // namespace proximity_control_task

namespace main_control_task {

auto run() -> void {
    while (true) {
        proximity_event_t event{};
        if (!proximity_sensor_wait_event(&event, portMAX_DELAY)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        std::array<char, 48> line{};
        const int written =
            std::snprintf(line.data(), line.size(), "proximity %s %s\r\n",
                          sensor_name(event.id),
                          event.object_detected ? "detected" : "clear");
        if (written <= 0) {
            continue;
        }
        const auto length = static_cast<std::size_t>(written);
        const auto capped = length < line.size() ? length : line.size() - 1U;
        static_cast<void>(serial_hardware_write(line.data(), capped));
    }
}

}  // namespace main_control_task
