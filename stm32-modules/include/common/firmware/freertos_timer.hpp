#pragma once

#include <functional>

#include "FreeRTOS.h"
#include "timers.h"

namespace freertos_timer {

class FreeRTOSTimer {
  public:
    /*
     * A software timer class. The priority of software timers in FreeRTOS is
     * customizable, and any tasks utilizing this timer should have either the
     * same priority or higher priority than them for execution.
     */
    using Callback = std::function<void()>;
    FreeRTOSTimer(const char* name, uint32_t time_ms, bool autoreload,
                  Callback callback)
        : _callback{std::move(callback)} {
        // pdMS_TO_TICKS converts milliseconds to ticks. This can only be used
        // for FreeRTOS tick rates less than 1000 Hz.
        _timer_period = pdMS_TO_TICKS(time_ms);
        _auto_reload = (autoreload) ? pdTRUE : pdFALSE;
        _timer = xTimerCreateStatic(name, _timer_period, _auto_reload, this,
                                    timer_callback, &_timer_buffer);
    }
    auto operator=(FreeRTOSTimer&) -> FreeRTOSTimer& = delete;
    auto operator=(FreeRTOSTimer&&) -> FreeRTOSTimer&& = delete;
    FreeRTOSTimer(FreeRTOSTimer&) = delete;
    FreeRTOSTimer(FreeRTOSTimer&&) = delete;
    ~FreeRTOSTimer() { xTimerDelete(_timer, 0); }

    auto start() -> bool { return xTimerStart(_timer, 0) == pdPASS; }

    auto stop() -> bool { return xTimerStop(_timer, 0) == pdPASS; }

    [[nodiscard]] auto active() const -> bool {
        return xTimerIsTimerActive(_timer) != pdFALSE;
    }

  private:
    TimerHandle_t _timer{};
    Callback _callback;
    StaticTimer_t _timer_buffer{};
    UBaseType_t _auto_reload = pdTRUE;
    TickType_t _timer_period = 0;

    static void timer_callback(TimerHandle_t xTimer) {
        auto* timer_id = pvTimerGetTimerID(xTimer);
        auto* instance = static_cast<FreeRTOSTimer*>(timer_id);
        instance->_callback();
    }
};

}  // namespace freertos_timer
