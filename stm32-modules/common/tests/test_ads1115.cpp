#include "catch2/catch.hpp"
#include "core/ads1115.hpp"
#include "test/test_ads1115_policy.hpp"

using namespace ads1115_test_policy;
using namespace ADS1115;

TEST_CASE("ADS1115 test policy functionality") {
    GIVEN("a test policy") {
        ADS1115TestPolicy policy;
        REQUIRE(policy._backing[0].lock_count == 0);
        REQUIRE(policy._backing[1].lock_count == 0);
        THEN("the handles don't show as initialized") {
            REQUIRE(!policy.ads1115_check_initialized(0));
            REQUIRE(!policy.ads1115_check_initialized(1));
        }
        WHEN("marking the ADC's as initialized") {
            policy.ads1115_mark_initialized(0);
            policy.ads1115_mark_initialized(1);
            THEN("the handles do show as initialized") {
                REQUIRE(policy.ads1115_check_initialized(0));
                REQUIRE(policy.ads1115_check_initialized(1));
            }
        }
        WHEN("unlocking a mutex") {
            policy.ads1115_get_lock(0);
            THEN("trying to unlock again doesn't work") {
                REQUIRE_THROWS(policy.ads1115_get_lock(0));
            }
            AND_WHEN("releasing it") {
                policy.ads1115_release_lock(0);
                THEN("count increases") {
                    REQUIRE(policy._backing[0].lock_count == 1);
                    REQUIRE(policy._backing[1].lock_count == 0);
                }
            }
        }
        WHEN("arming a read") {
            REQUIRE(policy._backing[0].read_armed == false);
            policy.ads1115_arm_for_read(0);
            THEN("the struct is properly armed") {
                REQUIRE(policy._backing[0].read_armed == true);
            }
            THEN("waiting for a pulse returns true") {
                REQUIRE(policy.ads1115_wait_for_pulse(123));
                AND_THEN("the struct is marked as read") {
                    REQUIRE(policy._backing[0].read_armed == false);
                }
            }
        }
        THEN("waiting for a pulse returns false") {
            REQUIRE(!policy.ads1115_wait_for_pulse(123));
        }
        WHEN("setting an i2c register to each ADC") {
            policy.ads1115_i2c_write_16(policy.addresses[0], 0, 0x1234);
            policy.ads1115_i2c_write_16(policy.addresses[1], 1, 0x567);
            THEN("the values are written") {
                REQUIRE(policy._backing[0].written.at(0) == 0x1234);
                REQUIRE(policy._backing[1].written.at(1) == 0x567);
            }
        }
    }
}

TEST_CASE("ADS1115 driver") {
    GIVEN("two unique ADC") {
        ADS1115TestPolicy policy;
        auto adc1 = ADC(policy.addresses[0], 0, policy);
        auto adc2 = ADC(policy.addresses[1], 1, policy);
        THEN("the ADC's are not initialized") {
            REQUIRE(!adc1.initialized());
            REQUIRE(!adc2.initialized());
        }
        WHEN("initializing ADC1") {
            auto mutex_count = policy._backing[0].lock_count;
            adc1.initialize();
            THEN("mutex lock increases") {
                REQUIRE(policy._backing[0].lock_count == mutex_count + 1);
                REQUIRE(!policy._backing[0].locked);
            }
            THEN("ADC1 was initialized") {
                REQUIRE(policy._backing[0].initialized);
                REQUIRE(policy._backing[0].written.size() == 3);
                // Low threshold
                REQUIRE(policy._backing[0].written.at(2) == 0);
                // Hi threshold
                REQUIRE(policy._backing[0].written.at(3) == 0x8000);
                // config address
                REQUIRE(policy._backing[0].written.at(1) == 0x45A0);
                AND_WHEN("initializing it again") {
                    policy._backing[0].written.clear();
                    adc1.initialize();
                    THEN("the registers are not re-written") {
                        REQUIRE(policy._backing[0].written.size() == 0);
                    }
                }
            }
            THEN("reading from invalid pin fails") {
                mutex_count = policy._backing[0].lock_count;
                auto ret = adc1.read(6);
                REQUIRE(std::holds_alternative<Error>(ret));
                REQUIRE(std::get<Error>(ret) == Error::ADCPin);
                AND_THEN("mutex is not incremented") {
                    REQUIRE(policy._backing[0].lock_count == mutex_count);
                }
            }
            THEN("reading from valid pin succeeds") {
                mutex_count = policy._backing[0].lock_count;
                auto ret = adc1.read(1);
                REQUIRE(std::holds_alternative<uint16_t>(ret));
                REQUIRE(std::get<uint16_t>(ret) == policy.READBACK_VALUE);
                AND_THEN("mutex is incremented") {
                    REQUIRE(policy._backing[0].lock_count == mutex_count + 1);
                }
            }
            THEN("reading from the uninitialized ADC still fails") {
                auto mutex_count = policy._backing[1].lock_count;
                auto ret = adc2.read(0);
                REQUIRE(std::holds_alternative<Error>(ret));
                REQUIRE(std::get<Error>(ret) == Error::ADCInit);
                AND_THEN("mutex is not incremented") {
                    REQUIRE(policy._backing[1].lock_count == mutex_count);
                }
            }
        }
        THEN("reading from uninitialized ADC fails") {
            auto mutex_count = policy._backing[0].lock_count;
            auto ret = adc1.read(0);
            REQUIRE(std::holds_alternative<Error>(ret));
            REQUIRE(std::get<Error>(ret) == Error::ADCInit);
            AND_THEN("mutex is not incremented") {
                REQUIRE(policy._backing[0].lock_count == mutex_count);
            }
        }
    }
    GIVEN("two ADC pointing to same ID") {
        ADS1115TestPolicy policy;
        auto adc1 = ADC(policy.addresses[0], 0, policy);
        auto adc2 = ADC(policy.addresses[0], 0, policy);
        WHEN("initializing ADC1") {
            adc1.initialize();
            THEN("the backing ADC is initialized") {
                REQUIRE(adc1.initialized());
                REQUIRE(policy._backing[0].initialized);
                REQUIRE(policy._backing[0].written.size() == 3);
            }
            AND_WHEN("initializing ADC2") {
                policy._backing[0].written.clear();
                adc2.initialize();
                THEN("the registers are not rewritten") {
                    REQUIRE(adc2.initialized());
                    REQUIRE(policy._backing[0].initialized);
                    REQUIRE(policy._backing[0].written.size() == 0);
                }
            }
        }
    }
}
