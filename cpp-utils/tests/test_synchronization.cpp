#include "catch2/catch.hpp"
#include "ot_utils/core/synchronization.hpp"

SCENARIO("lock works") {
    GIVEN("a mock semaphore") {
        struct MockSemaphore {
            MockSemaphore() {}
            MockSemaphore(const MockSemaphore &) = delete;
            MockSemaphore(const MockSemaphore &&) = delete;
            MockSemaphore &operator=(const MockSemaphore &) = delete;
            MockSemaphore &&operator=(const MockSemaphore &&) = delete;

            void acquire() { acq_count++; }
            void release() { rel_count++; }
            int get_count() { return 0; }

            int acq_count{0};
            int rel_count{0};
        };
        auto sem = MockSemaphore{};
        WHEN("used") {
            { auto l = ot_utils::synchronization::Lock{sem}; }
            THEN("acquire is called once") { REQUIRE(sem.acq_count == 1); }
            THEN("release is called once") { REQUIRE(sem.rel_count == 1); }
        }
    }
}
