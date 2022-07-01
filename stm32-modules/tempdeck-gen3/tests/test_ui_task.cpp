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
    }
}
