#include "simulator/system_thread.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stop_token>
#include <thread>

#include "core/xt1511.hpp"
#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/tasks.hpp"

using namespace system_thread;

struct SimSystemPolicy {
  private:
    static constexpr uint16_t PWM_MAX = 213;
    static constexpr std::size_t LED_BUFFER_SIZE =
        (SYSTEM_LED_COUNT * xt1511::SINGLE_PIXEL_BUF_SIZE) + 1;
    using LedBuffer = std::array<uint16_t, LED_BUFFER_SIZE>;
    bool serial_number_set = false;
    static constexpr std::size_t SYSTEM_SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number = {};
    errors::ErrorCode set_serial_number_return = errors::ErrorCode::NO_ERROR;

    LedBuffer::iterator _led_input_buf_itr = nullptr;
    LedBuffer _led_buffer = {};
    bool _led_active = false;

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
    // Functions for XT1511 setting
    auto start_send(LedBuffer& buffer) -> bool {
        if (_led_active) {
            return false;
        }
        _led_input_buf_itr = buffer.begin();
        _led_active = true;
        return true;
    }
    auto end_send() -> void { _led_active = false; }
    auto wait_for_interrupt(uint32_t timeout_ms) -> bool {
        if (!_led_active) {
            return false;
        }
        auto itr = _led_input_buf_itr;
        for (size_t i = 0; i < _led_buffer.size(); ++i, std::advance(itr, 1)) {
            _led_buffer.at(i) = *itr;
        }
        return true;
    }
    [[nodiscard]] auto get_max_pwm() -> uint16_t { return PWM_MAX; }
};

struct system_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimSystemTask::Queue()), task(SimSystemTask(queue)) {}
    SimSystemTask::Queue queue;
    SimSystemTask task;
};

template <size_t N>
static auto get_serial_number() -> std::optional<std::array<char, N>> {
    using RT = std::optional<std::array<char, N>>;
    constexpr const char serial_number_varname[] = "SERIAL_NUMBER";
    auto* env_p = std::getenv(serial_number_varname);
    if (!env_p || (strlen(env_p) == 0)) {
        return RT();
    }
    std::array<char, N> ret;
    memcpy(ret.data(), env_p, std::min(strlen(env_p), N));
    return RT(ret);
}

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    using namespace std::literals::chrono_literals;
    auto policy = SimSystemPolicy();

    // Populate the serial number on startup, if provided
    auto ret = get_serial_number<SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>();
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
