#include "simulator/system_thread.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <stop_token>
#include <thread>

#include "heater-shaker/errors.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/simulator_utils.hpp"
#include "systemwide.h"

using namespace system_thread;

struct SimSystemPolicy {
  private:
    bool serial_number_set = false;
    static constexpr std::size_t SYSTEM_SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number = {};
    errors::ErrorCode set_serial_number_return = errors::ErrorCode::NO_ERROR;
    uint16_t last_delay = 0;

  public:
    auto enter_bootloader() -> void { std::terminate(); }

    auto set_serial_number(
        std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> new_system_serial_number)
        -> errors::ErrorCode {
        // copy to system_serial_number
        auto copy_start = new_system_serial_number.begin();
        auto copy_length = static_cast<int>(new_system_serial_number.size());
        std::copy(copy_start, (copy_start + copy_length),
                  system_serial_number.begin());
        serial_number_set = true;
        return set_serial_number_return;
    }

    auto get_serial_number(void)
        -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> {
        if (serial_number_set) {
            return system_serial_number;
        } else {
            std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> empty_serial_number =
                {"EMPTYSN"};
            return empty_serial_number;
        }
    }
    auto start_set_led(LED_COLOR color, uint8_t pwm_setting)
        -> errors::ErrorCode {
        return errors::ErrorCode::NO_ERROR;
    }

    auto check_I2C_ready(void) -> bool { return true; }

    auto delay_time_ms(uint16_t time_ms) -> void { last_delay = time_ms; }

    auto test_get_last_delay() const -> uint16_t { return last_delay; }
};

struct system_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimSystemTask::Queue()), task(SimSystemTask(queue)) {}
    SimSystemTask::Queue queue;
    SimSystemTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    using namespace std::literals::chrono_literals;
    auto policy = SimSystemPolicy();

    // Populate the serial number on startup, if provided
    constexpr const char serial_var_name[] = "SERIAL_NUMBER";
    auto ret =
        simulator_utils::get_serial_number<SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>(
            serial_var_name);
    if (ret.has_value()) {
        static_cast<void>(policy.set_serial_number(ret.value()));
    }

    tcb->queue.set_stop_token(st);

    while (!st.stop_requested()) {
        try {
            tcb->task.run_once(policy);
        } catch (const SimSystemTask::Queue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto system_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, SimSystemTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb), &tcb->task);
}
