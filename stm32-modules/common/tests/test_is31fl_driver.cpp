#include <array>

#include "catch2/catch.hpp"
#include "core/is31fl_driver.hpp"
#include "test/test_is31fl_policy.hpp"

using namespace test_is31fl_policy;

TEST_CASE("IS31FL test policy works") {
    GIVEN("A new IS31FL test policy") {
        auto policy = TestIS31FLPolicy();
        THEN("all register locations are 0") {
            auto reg = GENERATE(0, 1, 10, 100);
            REQUIRE(policy.check_register(reg) == 0x00);
        }
        WHEN("writing registers") {
            auto data = std::array<uint8_t, 5>();
            data.fill(0xA5);
            policy.i2c_write(0xAB, 0, data);
            THEN("they are updated") {
                REQUIRE(policy.check_register(0) == 0xA5);
                REQUIRE(policy.check_register(1) == 0xA5);
                REQUIRE(policy.check_register(2) == 0xA5);
                REQUIRE(policy.check_register(3) == 0xA5);
                REQUIRE(policy.check_register(4) == 0xA5);
                REQUIRE(policy.check_register(5) == 0);
                REQUIRE(policy.last_address == 0xAB);
            }
            AND_THEN("ovewriting one") {
                auto single = std::array<uint8_t, 1>();
                single.fill(0xEF);
                policy.i2c_write(0x55, 3, single);
                THEN("it is updated") {
                    REQUIRE(policy.check_register(0) == 0xA5);
                    REQUIRE(policy.check_register(1) == 0xA5);
                    REQUIRE(policy.check_register(2) == 0xA5);
                    REQUIRE(policy.check_register(3) == 0xEF);
                    REQUIRE(policy.check_register(4) == 0xA5);
                    REQUIRE(policy.check_register(5) == 0);
                    REQUIRE(policy.last_address == 0x55);
                }
            }
        }
    }
}

TEST_CASE("IS31FL driver functionality") {
    GIVEN("a new IS31FL driver") {
        auto policy = TestIS31FLPolicy();
        constexpr uint8_t address = 0xD8;
        auto subject = is31fl::IS31FL<address>();
        THEN("it is uninitialized") { REQUIRE(!subject.initialized()); }
        WHEN("initializing the driver") {
            REQUIRE(subject.initialize(policy));
            THEN("it shows up as initialized") {
                REQUIRE(subject.initialized());
            }
            THEN("the shutdown register was written") {
                REQUIRE(policy.check_register(0) == 0x01);
            }
            THEN("the shutdown register was written") {
                REQUIRE(policy.check_register(0) == 0x01);
            }
            THEN("the subject is using the correct address") {
                REQUIRE(policy.last_address == address);
            }
        }
        WHEN("setting a few channels to nonzero pwm/current") {
            subject.initialize(policy);
            subject.set_pwm(0.3);
            subject.set_pwm(0, 1.0);
            subject.set_pwm(2, 0.5);
            subject.set_current(0, 1.0);
            subject.set_current(1, 0.5);
            subject.send_update(policy);
            THEN("the correct registers are updated") {
                REQUIRE(policy.check_register(1) != policy.check_register(2));
                REQUIRE(policy.check_register(1) == 0xFF);
                REQUIRE(policy.check_register(0x14) == 0x30);
                REQUIRE(policy.check_register(0x14) != policy.check_register(0x15));
            }
        }
    }
}