#include "catch2/catch.hpp"
#include "core/at24c0xc.hpp"
#include "test/test_at24c0xc_policy.hpp"

TEST_CASE("TestAT24C0XCPolicy functionality") {
    GIVEN("a test policy of 32 pages") {
        auto policy = TestAT24C0XCPolicy<32>();
        WHEN("writing a full page") {
            std::array<uint8_t, 9> buffer = {0, 0, 1, 2, 3, 4, 5, 6, 7};
            REQUIRE(policy.i2c_write(0, buffer));
            THEN("the internal buffer does not match what was written") {
                for (int i = 0; i < 8; ++i) {
                    DYNAMIC_SECTION(std::string("Buffer index ") << i) {
                        REQUIRE(policy._buffer[i] == 0);
                    }
                }
            }
            AND_WHEN("reading a full page") {
                std::array<uint8_t, 8> readback = {0};
                policy.i2c_read(0, readback);
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
            WHEN("writing a full page") {
                std::array<uint8_t, 9> buffer = {0, 0, 1, 2, 3, 4, 5, 6, 7};
                REQUIRE(policy.i2c_write(0, buffer));
                THEN("the internal buffer matches what was written") {
                    for (int i = 0; i < 8; ++i) {
                        DYNAMIC_SECTION(std::string("Buffer index ") << i) {
                            REQUIRE(policy._buffer[i] == buffer[i + 1]);
                        }
                    }
                }
                AND_WHEN("reading a full page") {
                    std::array<uint8_t, 8> readback = {0};
                    policy.i2c_read(0, readback);
                    THEN("the returned buffer matches what was written") {
                        for (int i = 0; i < 8; ++i) {
                            DYNAMIC_SECTION(std::string("Buffer index ") << i) {
                                REQUIRE(readback[i] == buffer[i + 1]);
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

TEST_CASE("AT24C0XC class functionality") {
    using namespace at24c0xc;
    constexpr const size_t pages = 32;
    constexpr const uint8_t address = 0b1010100;
    GIVEN("a 32-page AT24C0xC") {
        auto policy = TestAT24C0XCPolicy<pages>();
        auto eeprom = AT24C0xC<pages, address>();
        THEN("size is 32 * 8 bytes") { REQUIRE(eeprom.size() == pages * 8); }
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
        WHEN("reading from page 35") {
            auto ret = eeprom.read_value<double>(35, policy);
            THEN("nothing is read") { REQUIRE(!ret.has_value()); }
        }
    }
}
