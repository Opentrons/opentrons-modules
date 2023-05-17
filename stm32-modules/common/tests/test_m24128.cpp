#include "catch2/catch.hpp"
#include "core/m24128.hpp"
#include "test/test_m24128_policy.hpp"

using namespace m24128_test_policy;

TEST_CASE("TestM24128Policy functionality") {
    GIVEN("a test policy") {
        auto policy = TestM24128Policy();
        WHEN("writing 8 bytes") {
            std::array<uint8_t, 10> buffer = {0, 0, 0, 1, 2, 3, 4, 5, 6, 7};
            REQUIRE(policy.i2c_write(0, buffer.begin(), buffer.size()));
            THEN("the internal buffer does not match what was written") {
                for (int i = 0; i < 8; ++i) {
                    DYNAMIC_SECTION(std::string("Buffer index ") << i) {
                        REQUIRE(policy._buffer[i] == 0);
                    }
                }
            }
            AND_WHEN("reading 8 bytes") {
                std::array<uint8_t, 8> readback = {0, 0, 0, 0, 0, 0, 0, 0};
                policy.i2c_write(0, readback.begin(), 2);
                policy.i2c_read(0, readback.begin(), readback.size());
                THEN("the returned buffer does not match what was written") {
                    for (int i = 0; i < 8; ++i) {
                        DYNAMIC_SECTION(std::string("Buffer index ") << i) {
                            REQUIRE(readback[i] == 0);
                        }
                    }
                }
            }
        }
        GIVEN("write protect disabled") {
            policy.set_write_protect(false);
            WHEN("writing 8 bytes") {
                std::array<uint8_t, 10> buffer = {0, 0, 0, 1, 2, 3, 4, 5, 6, 7};
                REQUIRE(policy.i2c_write(0, buffer.begin(), buffer.size()));
                THEN("the internal buffer matches what was written") {
                    for (int i = 0; i < 8; ++i) {
                        DYNAMIC_SECTION(std::string("Buffer index ") << i) {
                            REQUIRE(policy._buffer[i] == buffer[i + 2]);
                        }
                    }
                }
                AND_WHEN("reading bytes back") {
                    std::array<uint8_t, 8> readback = {0};
                    policy.i2c_write(0, readback.begin(), 2);
                    policy.i2c_read(0, readback.begin(), readback.size());
                    THEN("the returned buffer matches what was written") {
                        for (int i = 0; i < 8; ++i) {
                            DYNAMIC_SECTION(std::string("Buffer index ") << i) {
                                REQUIRE(readback[i] == buffer[i + 2]);
                            }
                        }
                    }
                }
            }
        }
    }
}

struct TwoFloats {
    float a, b;
};

TEST_CASE("M24128 class functionality") {
    using namespace m24128;
    constexpr const uint8_t address = 0b1010100;
    GIVEN("an M24128") {
        auto policy = TestM24128Policy();
        auto eeprom = M24128<address>();
        WHEN("writing a float to page 0") {
            float value = 10.0;
            eeprom.write_value(0, value, policy);
            AND_WHEN("reading back the stored value") {
                auto readback = eeprom.read_value<float>(0, policy);
                THEN("the same number is returned") {
                    REQUIRE(readback.has_value());
                    REQUIRE(readback.value() == value);
                }
            }
            AND_WHEN("reading back the stored value as a double") {
                auto readback = eeprom.read_value<double>(0, policy);
                THEN("the returned number is wrong") {
                    REQUIRE(readback.has_value());
                    REQUIRE(readback.value() != value);
                }
            }
        }
        WHEN("writing a struct to page 4") {
            TwoFloats value = {.a = 1.0, .b = 2.0};
            eeprom.write_value(4, value, policy);
            AND_WHEN("reading back the stored value") {
                auto readback = eeprom.read_value<TwoFloats>(4, policy);
                THEN("the same number is returned") {
                    REQUIRE(readback.has_value());
                    REQUIRE(readback.value().a == value.a);
                    REQUIRE(readback.value().b == value.b);
                }
            }
        }
        WHEN("reading from page 200") {
            auto ret = eeprom.read_value<double>(200, policy);
            THEN("nothing is read") { REQUIRE(!ret.has_value()); }
        }
    }
}
