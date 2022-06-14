#include "catch2/catch.hpp"
#include "core/ads1115.hpp"
#include "test/test_ads1115_policy.hpp"

using namespace ads1115_test_policy;
using namespace ADS1115;

TEST_CASE("ADS1115 test policy functionality") {
    GIVEN("a test policy") {
        ADS1115TestPolicy policy;
        REQUIRE(policy._lock_count == 0);
        THEN("the handle doesn't show as initialized") {
            REQUIRE(!policy.ads1115_check_initialized());
        }
        WHEN("marking the ADC as initialized") {
            policy.ads1115_mark_initialized();
            THEN("the handles do show as initialized") {
                REQUIRE(policy.ads1115_check_initialized());
            }
        }
        WHEN("acquiring a mutex") {
            policy.ads1115_get_lock();
            THEN("trying to acquire again doesn't work") {
                REQUIRE_THROWS(policy.ads1115_get_lock());
            }
            AND_WHEN("releasing it") {
                policy.ads1115_release_lock();
                THEN("count increases") {
                    REQUIRE(policy._lock_count == 1);
                }
            }
        }
        WHEN("arming a read") {
            REQUIRE(policy._read_armed == false);
            policy.ads1115_arm_for_read();
            THEN("the struct is properly armed") {
                REQUIRE(policy._read_armed == true);
            }
            THEN("waiting for a pulse returns true") {
                REQUIRE(policy.ads1115_wait_for_pulse(123));
                AND_THEN("the struct is marked as read") {
                    REQUIRE(policy._read_armed == false);
                }
            }
        }
        THEN("waiting for a pulse returns false") {
            REQUIRE(!policy.ads1115_wait_for_pulse(123));
        }
        WHEN("writing I2C registers") {
            policy.ads1115_i2c_write_16(0, 0x1234);
            policy.ads1115_i2c_write_16(1, 0x567);
            THEN("the values are written") {
                REQUIRE(policy._written.at(0) == 0x1234);
                REQUIRE(policy._written.at(1) == 0x567);
            }
        }
    }
}

TEST_CASE("ADS1115 driver") {
    GIVEN("two unique ADC") {
        ADS1115TestPolicy policy1;
        ADS1115TestPolicy policy2;
        auto adc1 = ADC(policy1);
        auto adc2 = ADC(policy2);
        THEN("the ADC's are not initialized") {
            REQUIRE(!adc1.initialized());
            REQUIRE(!adc2.initialized());
        }
        WHEN("initializing ADC1") {
            auto mutex_count = policy1._lock_count;
            adc1.initialize();
            THEN("mutex lock increases") {
                REQUIRE(policy1._lock_count == mutex_count + 1);
                REQUIRE(!policy1._locked);
            }
            THEN("ADC1 was initialized") {
                REQUIRE(adc1.initialized());
                REQUIRE(policy1._initialized);
                REQUIRE(policy1._written.size() == 3);
                // Low threshold
                REQUIRE(policy1._written.at(2) == 0);
                // Hi threshold
                REQUIRE(policy1._written.at(3) == 0x8000);
                // config address
                REQUIRE(policy1._written.at(1) == 0x45A0);
                AND_WHEN("initializing it again") {
                    policy1._written.clear();
                    adc1.initialize();
                    THEN("the registers are not re-written") {
                        REQUIRE(policy1._written.size() == 0);
                    }
                }
            }
            THEN("ADC2 was not initialized") {
                REQUIRE(!adc2.initialized());
            }
            THEN("reading from invalid pin fails") {
                mutex_count = policy1._lock_count;
                auto ret = adc1.read(6);
                REQUIRE(std::holds_alternative<Error>(ret));
                REQUIRE(std::get<Error>(ret) == Error::ADCPin);
                AND_THEN("mutex is not incremented") {
                    REQUIRE(policy1._lock_count == mutex_count);
                }
            }
            THEN("reading from valid pin succeeds") {
                mutex_count = policy1._lock_count;
                auto ret = adc1.read(1);
                REQUIRE(std::holds_alternative<uint16_t>(ret));
                REQUIRE(std::get<uint16_t>(ret) == policy1.READBACK_VALUE);
                AND_THEN("mutex is incremented") {
                    REQUIRE(policy1._lock_count == mutex_count + 1);
                }
            }
            THEN("reading from the uninitialized ADC still fails") {
                auto mutex_count = policy2._lock_count;
                auto ret = adc2.read(0);
                REQUIRE(std::holds_alternative<Error>(ret));
                REQUIRE(std::get<Error>(ret) == Error::ADCInit);
                AND_THEN("mutex is not incremented") {
                    REQUIRE(policy2._lock_count == mutex_count);
                }
            }
        }
        THEN("reading from uninitialized ADC fails") {
            auto mutex_count = policy1._lock_count;
            auto ret = adc1.read(0);
            REQUIRE(std::holds_alternative<Error>(ret));
            REQUIRE(std::get<Error>(ret) == Error::ADCInit);
            AND_THEN("mutex is not incremented") {
                REQUIRE(policy1._lock_count == mutex_count);
            }
        }
    }
    GIVEN("two ADC pointing to same ID") {
        ADS1115TestPolicy policy1;
        auto adc1 = ADC(policy1);
        auto adc2 = ADC(policy1);
        WHEN("initializing ADC1") {
            adc1.initialize();
            THEN("the backing ADC is initialized") {
                REQUIRE(adc1.initialized());
                REQUIRE(policy1._initialized);
                REQUIRE(policy1._written.size() == 3);
            }
            AND_WHEN("initializing ADC2") {
                policy1._written.clear();
                adc2.initialize();
                THEN("the registers are not rewritten") {
                    REQUIRE(adc2.initialized());
                    REQUIRE(policy1._initialized);
                    REQUIRE(policy1._written.size() == 0);
                }
            }
            THEN("ADC2 can tell it was initialized") {
                REQUIRE(adc2.initialized());
            }
        }
    }
}
