#include "catch2/catch.hpp"
#include "test/test_ui_policy.hpp"
#include "test/test_tasks.hpp"

TEST_CASE("ui periodic updates") {
    auto *tasks = tasks::BuildTasks();
    TestUIPolicy policy;
    WHEN("sending an UpdateUIMessage") {
        auto msg = messages::UpdateUIMessage;
        REQUIRE(tasks->_ui_queue.try_send(msg));
    }
}
