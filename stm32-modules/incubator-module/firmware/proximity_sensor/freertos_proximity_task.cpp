#include <array>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
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

constexpr size_t SERIAL_COMMAND_BUFFER_SIZE = 32;

using ProximityQueue = ::tasks::FirmwareTasks::ProximityQueue;
using QueueAggregator = ::tasks::FirmwareTasks::QueueAggregator;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static ProximityQueue proximity_queue{1, "Proximity Queue"};

static auto write_line(const char* line, int length) -> void {
    if (length > 0) {
        static_cast<void>(serial_hardware_write(
            line, static_cast<std::size_t>(length)));
    }
}

static auto command_matches(const char* command, size_t length,
                            const char* code) -> bool {
    const auto code_length = std::strlen(code);
    return length >= code_length &&
           std::memcmp(command, code, code_length) == 0 &&
           (length == code_length ||
            std::isspace(static_cast<unsigned char>(command[code_length])));
}

static auto handle_proximity_debug_command() -> void {
    proximity_pin_registers_t regs{};
    if (!proximity_sensor_read_registers(PROXIMITY_SENSOR_PC2, &regs)) {
        return;
    }
    std::array<char, 80> response{};
    const int written = std::snprintf(
        response.data(), response.size(),
        "M113.D PC2 IDR:%u MODER:%u PUPDR:%u EXTI:%u OK\r\n",
        static_cast<unsigned>(regs.idr), static_cast<unsigned>(regs.moder),
        static_cast<unsigned>(regs.pupdr),
        static_cast<unsigned>(regs.exti_imr));
    if (written > 0 && static_cast<std::size_t>(written) < response.size()) {
        write_line(response.data(), written);
    }
}

static auto handle_proximity_serial_command() -> void {
    const int pc2 = proximity_sensor_read_raw(PROXIMITY_SENSOR_PC2) ? 1 : 0;
    std::array<char, 32> response{};
    const int written = std::snprintf(response.data(), response.size(),
                                      "M113 PC2:%d OK\r\n", pc2);
    if (written > 0 && static_cast<std::size_t>(written) < response.size()) {
        write_line(response.data(), written);
    }
}

static auto handle_serial_line(const char* command, size_t length) -> void {
    while (length > 0 &&
           std::isspace(static_cast<unsigned char>(command[length - 1]))) {
        --length;
    }
    if (command_matches(command, length, "M113.D")) {
        handle_proximity_debug_command();
        return;
    }
    if (command_matches(command, length, "M113")) {
        handle_proximity_serial_command();
        return;
    }
    static constexpr char error[] = "ERR003:unhandled gcode OK\r\n";
    write_line(error, static_cast<int>(sizeof(error) - 1));
}

static auto read_serial_command(char* command, size_t& length,
                                bool& overflow) -> bool {
    auto read_byte = false;
    uint8_t serial_byte = 0;
    while (serial_hardware_read_byte(&serial_byte)) {
        read_byte = true;
        if (serial_byte == '\r' || serial_byte == '\n') {
            if (length > 0 || overflow) {
                if (overflow) {
                    static constexpr char error[] =
                        "ERR003:gcode line too long OK\r\n";
                    write_line(error, static_cast<int>(sizeof(error) - 1));
                } else {
                    handle_serial_line(command, length);
                }
                length = 0;
                overflow = false;
            }
            continue;
        }
        if (length < SERIAL_COMMAND_BUFFER_SIZE) {
            command[length++] = static_cast<char>(serial_byte);
        } else {
            overflow = true;
        }
    }
    return read_byte;
}

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
    std::array<char, SERIAL_COMMAND_BUFFER_SIZE> serial_command{};
    size_t serial_command_length = 0;
    bool serial_command_overflow = false;
    auto last_sample = xTaskGetTickCount();
    while (true) {
        const auto now = xTaskGetTickCount();
        if ((now - last_sample) >= pdMS_TO_TICKS(PROXIMITY_SAMPLE_MS)) {
            proximity_sensor_poll();
            last_sample = now;
        }
        const auto read_byte = read_serial_command(
            serial_command.data(), serial_command_length,
            serial_command_overflow);
        auto handled_message = false;
        while (aggregator != nullptr && proximity_queue.try_recv(&message)) {
            handled_message = true;
            if (const auto* request =
                    std::get_if<messages::GetProximityStateMessage>(&message)) {
                handle_state_request(*request, aggregator);
            }
        }
        if (!handled_message && !read_byte) {
            vTaskDelay(pdMS_TO_TICKS(1));
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
