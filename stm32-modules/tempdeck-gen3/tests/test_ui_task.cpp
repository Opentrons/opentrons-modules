#include <algorithm>

#include "catch2/catch.hpp"
#include "test/test_tasks.hpp"
#include "test/test_ui_policy.hpp"

TEST_CASE("ui periodic updates") {
    auto *tasks = tasks::BuildTasks();
    TestUIPolicy policy;
    WHEN("sending an UpdateUIMessage") {
        auto msg = messages::UpdateUIMessage();
        REQUIRE(tasks->_ui_queue.try_send(msg));
        tasks->_ui_task.run_once(policy);
        THEN("the message is consumed") {
            REQUIRE(!tasks->_ui_queue.has_message());
        }
        THEN("the heartbeat LED is updated") {
            REQUIRE(policy._heartbeat_set_count == 1);
        }
        THEN("the front lights are set to white") {
            auto white_on = std::ranges::all_of(
                ui_task::white_channels.cbegin(),
                ui_task::white_channels.cend(), [&policy](size_t c) {
                    return policy.check_register(c + 0x14) != 0x00;
                });
            REQUIRE(white_on);
            auto blue_off = std::ranges::all_of(
                ui_task::blue_channels.cbegin(), ui_task::blue_channels.cend(),
                [&policy](size_t c) {
                    return policy.check_register(c + 0x14) == 0x00;
                });
            REQUIRE(blue_off);
            auto red_off = std::ranges::all_of(
                ui_task::red_channels.cbegin(), ui_task::red_channels.cend(),
                [&policy](size_t c) {
                    return policy.check_register(c + 0x14) == 0x00;
                });
            REQUIRE(red_off);
            auto green_off = std::ranges::all_of(
                ui_task::green_channels.cbegin(),
                ui_task::green_channels.cend(), [&policy](size_t c) {
                    return policy.check_register(c + 0x14) == 0x00;
                });
            REQUIRE(green_off);
        }
    }
}
