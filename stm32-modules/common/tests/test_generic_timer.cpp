
#include "catch2/catch.hpp"
#include "core/timer.hpp"
#include "test/test_timer_handle.hpp"

using namespace test_timer_handle;
using namespace timer;

SCENARIO("TestTimerHandle works") {
    GIVEN("a TestTimerHandle with period 100ms and autoreload") {
        constexpr uint32_t PERIOD = 100;
        InterruptCounter itr;
        TestTimerHandle timer("Test1", PERIOD, true, itr.provide_callback());
        REQUIRE(itr._count == 0);
        WHEN("starting the timer") {
            timer.start();
            AND_WHEN("incremementing by 100ms") {
                timer.tick(PERIOD);
                THEN("the interrupt is fired") {
                    REQUIRE(itr._count == 1);
                    REQUIRE(timer.active());
                    REQUIRE(timer.remaining_time() == PERIOD);
                }
                AND_WHEN("incremementing by another 100ms") {
                    timer.tick(PERIOD);
                    THEN("the interrupt is fired") {
                        REQUIRE(itr._count == 2);
                        REQUIRE(timer.active());
                        REQUIRE(timer.remaining_time() == PERIOD);
                    }
                }
            }
            AND_WHEN("incremementing by 99ms") {
                timer.tick(PERIOD - 1);
                THEN("the interrupt is not fired") {
                    REQUIRE(itr._count == 0);
                    REQUIRE(timer.active());
                    REQUIRE(timer.remaining_time() == 1);
                }
            }
            AND_WHEN("incremementing by 1ms") {
                timer.tick(1);
                THEN("the interrupt is not fired") {
                    REQUIRE(itr._count == 0);
                    REQUIRE(timer.active());
                    REQUIRE(timer.remaining_time() == PERIOD - 1);
                }
            }
            AND_WHEN("incremementing by 200ms") {
                timer.tick(PERIOD * 2);
                THEN("the interrupt is fired twice") {
                    REQUIRE(itr._count == 2);
                    REQUIRE(timer.active());
                    REQUIRE(timer.remaining_time() == PERIOD);
                }
            }
            AND_WHEN("incremementing by 250ms") {
                timer.tick(static_cast<uint32_t>(PERIOD * 2.5));
                THEN("the interrupt is fired twice") {
                    REQUIRE(itr._count == 2);
                    REQUIRE(timer.active());
                    REQUIRE(timer.remaining_time() == PERIOD / 2);
                }
                AND_WHEN("turning the timer off") {
                    timer.stop();
                    THEN("the timer is off") {
                        REQUIRE(!timer.active());
                        REQUIRE(timer.remaining_time() == 0);
                    }
                }
            }
        }
    }
    GIVEN("a TestTimerHandle with a period 100ms and no autoreload") {
        constexpr uint32_t PERIOD = 100;
        InterruptCounter itr;
        TestTimerHandle timer("Test1", PERIOD, false, itr.provide_callback());
        REQUIRE(itr._count == 0);
        WHEN("starting the timer") {
            timer.start();
            AND_WHEN("incremementing by 100ms") {
                timer.tick(PERIOD);
                THEN("the interrupt is fired and the timer is off") {
                    REQUIRE(itr._count == 1);
                    REQUIRE(!timer.active());
                    REQUIRE(timer.remaining_time() == 0);
                }
                AND_WHEN("incremementing by another 100ms") {
                    timer.tick(PERIOD);
                    THEN("the interrupt is not fired") {
                        REQUIRE(itr._count == 1);
                        REQUIRE(!timer.active());
                        REQUIRE(timer.remaining_time() == 0);
                    }
                }
            }
            AND_WHEN("incremementing by 200ms") {
                timer.tick(PERIOD * 2);
                THEN("the interrupt is fired and the timer is off") {
                    REQUIRE(itr._count == 1);
                    REQUIRE(!timer.active());
                    REQUIRE(timer.remaining_time() == 0);
                }
            }
        }
    }
}

SCENARIO("GenericTimer class works") {
    GIVEN("a GenericTimer with a period of 100ms and autoreload enabled") {
        constexpr uint32_t PERIOD = 100;
        InterruptCounter itr;
        auto timer = GenericTimer<TestTimerHandle>("Test2", PERIOD, true,
                                                   itr.provide_callback());

        REQUIRE(itr._count == 0);
        WHEN("manually invoking the callback") {
            timer.callback();
            THEN("the count is incremented") { REQUIRE(itr._count == 1); }
        }
        WHEN("starting the timer") {
            REQUIRE(timer.start());
            AND_WHEN("incrementing by 100ms") {
                timer.get_handle().tick(PERIOD);
                THEN("the interrupt is triggered and timer is active") {
                    REQUIRE(itr._count == 1);
                    REQUIRE(timer.active());
                    REQUIRE(timer.get_handle().remaining_time() == PERIOD);
                }
            }
        }
    }
    GIVEN("a GenericTimer with a period of 100ms and autoreload disabled") {
        constexpr uint32_t PERIOD = 100;
        InterruptCounter itr;
        auto timer = GenericTimer<TestTimerHandle>("Test2", PERIOD, false,
                                                   itr.provide_callback());

        REQUIRE(itr._count == 0);
        WHEN("starting the timer") {
            REQUIRE(timer.start());
            AND_WHEN("incrementing by 100ms") {
                timer.get_handle().tick(PERIOD);
                THEN("the interrupt is triggered and timer is inactive") {
                    REQUIRE(itr._count == 1);
                    REQUIRE(!timer.active());
                    REQUIRE(timer.get_handle().remaining_time() == 0);
                }
            }
        }
    }
}
