#pragma once

#include <functional>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

constexpr auto ROUNDED_DIV(uint32_t A, uint32_t B) noexcept -> uint32_t {
    return (((A) + ((B) / 2)) / (B));
}

constexpr auto TICKS_TO_MS(uint32_t ticks) noexcept -> uint32_t {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    return ROUNDED_DIV((ticks) * 1000, (uint32_t)configTICK_RATE_HZ);
}

namespace ot_utils::freertos_timer {

class FreeRTOSTimer {
  public:
    /*
     * A software timer class. The priority of software timers in FreeRTOS is
     * currently set to 6. Any tasks utilizing this timer should have either the
     * same priority or higher priority than 6 for execution.
     */
    using Callback = std::function<void()>;
	//NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    FreeRTOSTimer(const char* name, Callback&& callback, uint32_t period_ms)
		//NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
        : FreeRTOSTimer(name, std::forward<Callback>(callback), true,
                        period_ms) {}

    FreeRTOSTimer(const char* name, Callback&& callback, bool autoreload,
                  uint32_t period_ms)
        : callback{std::move(callback)} {
        timer = xTimerCreateStatic(name, pdMS_TO_TICKS(period_ms),
                                   static_cast<UBaseType_t>(autoreload), this,
                                   timer_callback, &timer_buffer);
    }

    auto operator=(FreeRTOSTimer&) -> FreeRTOSTimer& = delete;
    auto operator=(FreeRTOSTimer&&) -> FreeRTOSTimer&& = delete;
    FreeRTOSTimer(FreeRTOSTimer&) = delete;
    FreeRTOSTimer(FreeRTOSTimer&&) = delete;
    ~FreeRTOSTimer() { xTimerDelete(timer, 0); }

    auto is_running() -> bool { return (xTimerIsTimerActive(timer) == pdTRUE); }

    void update_callback(Callback&& new_callback) {
        callback = std::move(new_callback);
    }

    void update_period(uint32_t period_ms) {
        // Update the period of the timer and suppress the freertos behavior
        // that if the timer is currently stopped, changing its period activates
        // it.
        auto is_active = is_running();
        const TickType_t blocking_ticks = is_active ? 1 : 0;
        xTimerChangePeriod(timer, pdMS_TO_TICKS(period_ms), blocking_ticks);
        if (!is_active) {
            stop();
            vTaskDelay(1);
        }
    }

    auto get_remaining_time() -> uint32_t {
        /*
        The time in ms remaining before this timer expires and the callback is executed.
        */
        if (!is_running()) { return uint32_t(0); }
        return TICKS_TO_MS(xTimerGetExpiryTime( timer ) - xTaskGetTickCount());
    }


    void start() { xTimerStart(timer, 1); }

    void stop() { xTimerStop(timer, 1); }

    auto start_from_isr() -> bool {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        auto ret = xTimerStartFromISR(timer, &xHigherPriorityTaskWoken);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return ret == pdPASS;
    }

    auto stop_from_isr() -> bool {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        auto ret = xTimerStopFromISR(timer, &xHigherPriorityTaskWoken);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return ret == pdPASS;
    }

  private:
    TimerHandle_t timer{};
    Callback callback;
    StaticTimer_t timer_buffer{};

    static void timer_callback(TimerHandle_t xTimer) {
        auto* timer_id = pvTimerGetTimerID(xTimer);
        auto* instance = static_cast<FreeRTOSTimer*>(timer_id);
        instance->callback();
    }
};

}  // namespace ot_utils::freertos_timer
