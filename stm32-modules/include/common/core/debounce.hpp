#pragma once
#include <atomic>

namespace debouncer {

struct Debouncer {
    int holdoff_cnt = 1;
    std::atomic_bool state = false;
    std::atomic_bool state_bounce = false;
    int cnt = 0;

    auto debounce_update(bool new_state) -> void {
        // only set the state if the bounce matches the current gpio_is_set
        // on the first state change it won't match but on the second tick it
        // will and we can set it to the new state.

        if (new_state == state_bounce) {
            cnt++;
            if (cnt == holdoff_cnt) {
                // update state only when cnt reaches the holdoff count
                state = new_state;
                cnt = 0;
            }
        } else {
            // reset count every time the mismatched
            cnt = 0;
        }
        // state bounce is updated each time
        state_bounce = new_state;
    }
    [[nodiscard]] auto debounce_state() const -> bool { return state.load(); }
    auto reset() -> void {
        state = false;
        state_bounce = false;
        cnt = 0;
    }
};

}  // namespace debouncer
