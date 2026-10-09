#include "firmware/freertos_fdc1004_task.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <variant>

#include "FreeRTOS.h"
#include "firmware/fdc1004_hardware.h"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/proximity_sensor.h"
#include "firmware/serial_hardware.h"
#include "incubator-module/fdc1004.hpp"
#include "incubator-module/messages.hpp"
#include "task.h"

namespace fdc1004::tasks {

static constexpr uint16_t CONFIG = 10;
static constexpr uint32_t MEASUREMENT_TIMEOUT_MS = 250;
static constexpr size_t SERIAL_COMMAND_BUFFER_SIZE = 32;

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
    if (!aggregator->send_to_address(response,
                                     ::tasks::FirmwareTasks::HostCommsAddress,
                                     pdMS_TO_TICKS(10))) {
        std::cerr << "Failed to send FDC1004 readings to host comms"
                  << std::endl;
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

static auto handle_proximity_serial_command() -> void {
    char response[48] = {};
    const int pc2 = proximity_sensor_read_raw(PROXIMITY_SENSOR_PC2) ? 1 : 0;
    const int response_length = std::snprintf(
        response, sizeof(response), "M113 PC2:%d OK\r\n", pc2);
    if (response_length > 0 &&
        static_cast<size_t>(response_length) < sizeof(response)) {
        static_cast<void>(serial_hardware_write(
            response, static_cast<size_t>(response_length)));
    }
}

static auto handle_proximity_debug_command() -> void {
    proximity_pin_registers_t regs{};
    if (!proximity_sensor_read_registers(PROXIMITY_SENSOR_PC2, &regs)) {
        return;
    }
    char response[80] = {};
    const int response_length = std::snprintf(
        response, sizeof(response),
        "M113.D PC2 IDR:%u MODER:%u PUPDR:%u EXTI:%u OK\r\n",
        static_cast<unsigned>(regs.idr), static_cast<unsigned>(regs.moder),
        static_cast<unsigned>(regs.pupdr),
        static_cast<unsigned>(regs.exti_imr));
    if (response_length > 0 &&
        static_cast<size_t>(response_length) < sizeof(response)) {
        static_cast<void>(serial_hardware_write(
            response, static_cast<size_t>(response_length)));
    }
}

static auto handle_serial_command(const char* command, size_t length,
                                  fdc1004::FDC1004& sensor,
                                  bool sensor_initialized) -> void {
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
    if (!command_matches(command, length, "M111")) {
        static constexpr char error[] = "ERR003:unhandled gcode OK\r\n";
        static_cast<void>(serial_hardware_write(error, sizeof(error) - 1));
        return;
    }

    double channel_1 = 0.0;
    double channel_2 = 0.0;
    double channel_3 = 0.0;
    double channel_4 = 0.0;
    if (!read_all_channels(sensor, sensor_initialized, channel_1, channel_2,
                           channel_3, channel_4)) {
        static constexpr char error[] =
            "ERR601:capacitive sensor reading failed OK\r\n";
        static_cast<void>(serial_hardware_write(error, sizeof(error) - 1));
        return;
    }

    char response[128] = {};
    const int response_length =
        std::snprintf(response, sizeof(response),
                      "M111 C1:%.1f C2:%.1f C3:%.1f C4:%.1f OK\r\n", channel_1,
                      channel_2, channel_3, channel_4);
    if (response_length > 0 &&
        static_cast<size_t>(response_length) < sizeof(response)) {
        static_cast<void>(serial_hardware_write(
            response, static_cast<size_t>(response_length)));
    }
}

auto run(QueueAggregator* aggregator, i2c::hardware::I2C* i2c_bus_handle)
    -> void {
    if (aggregator == nullptr || i2c_bus_handle == nullptr) {
        std::cerr << "FDC1004 task received an invalid startup argument"
                  << std::endl;
        vTaskSuspend(nullptr);
        return;
    }

    fdc1004::FDC1004 capacitive_sensor{i2c_bus_handle};

    capacitive_queue.provide_handle(xTaskGetCurrentTaskHandle());
    if (!aggregator->register_queue(capacitive_queue)) {
        std::cerr << "Failed to register FDC1004 request queue" << std::endl;
        vTaskSuspend(nullptr);
        return;
    }
    capacitive_queue.set_ready();

    const bool sensor_initialized =
        fdc1004_hardware_init(I2C_BUS_1, 0, CONFIG) &&
        capacitive_sensor.configure_single_ended(1, CONFIG) &&
        capacitive_sensor.configure_single_ended(2, CONFIG) &&
        capacitive_sensor.configure_single_ended(3, CONFIG);
    if (!sensor_initialized) {
        std::cerr << "FDC1004 hardware initialization failed" << std::endl;
    } else {
        std::cout << "FDC1004 task ready for M111" << std::endl;
    }

    messages::CapacitiveMessage message{};
    char serial_command[SERIAL_COMMAND_BUFFER_SIZE] = {};
    size_t serial_command_length = 0;
    bool serial_command_overflow = false;
    for (;;) {
        bool did_work = false;
        uint8_t serial_byte = 0;
        if (serial_hardware_read_byte(&serial_byte)) {
            did_work = true;
            if (serial_byte == '\r' || serial_byte == '\n') {
                if (serial_command_length > 0 || serial_command_overflow) {
                    if (serial_command_overflow) {
                        static constexpr char error[] =
                            "ERR003:gcode line too long OK\r\n";
                        static_cast<void>(
                            serial_hardware_write(error, sizeof(error) - 1));
                    } else {
                        handle_serial_command(
                            serial_command, serial_command_length,
                            capacitive_sensor, sensor_initialized);
                    }
                    serial_command_length = 0;
                    serial_command_overflow = false;
                }
            } else if (serial_command_length < sizeof(serial_command)) {
                serial_command[serial_command_length++] =
                    static_cast<char>(serial_byte);
            } else {
                serial_command_overflow = true;
            }
        }

        if (capacitive_queue.try_recv(&message)) {
            did_work = true;
            if (const auto* request =
                    std::get_if<messages::GetCapacitiveStateMessage>(
                        &message)) {
                handle_state_request(*request, capacitive_sensor, aggregator,
                                     sensor_initialized);
            }
        }
        if (!did_work) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

}  // namespace fdc1004::tasks
