#include <cstdbool>
#include <iostream>

#include "catch2/catch.hpp"
#include "core/xt1511.hpp"

using namespace xt1511;

template <size_t N>
class TestXT1511Policy {
  public:
    static constexpr size_t BITS_PER_PIXEL = xt1511::SINGLE_PIXEL_BUF_SIZE;
    using InBuffer = std::array<uint16_t, (BITS_PER_PIXEL * N) + 1>;

    TestXT1511Policy() = delete;

    TestXT1511Policy(uint16_t max_pwm)
        : _max_pwm(max_pwm),
          _buffer{0},
          _input_buffer_begin(),
          _input_buffer_end(),
          _active(false) {}

    auto start_send(InBuffer& buffer) -> bool {
        if (_active) {
            return false;
        }
        _input_buffer_begin = buffer.begin();
        _input_buffer_end = buffer.end();
        _active = true;
        return true;
    }

    auto end_send() -> void { _active = false; }

    // Simulate end of a transfer by copying all data
    auto wait_for_interrupt(uint32_t timeout) -> bool {
        auto* my_itr = _buffer.begin();
        for (auto* in_itr = _input_buffer_begin; in_itr != _input_buffer_end;
             std::advance(in_itr, 1), std::advance(my_itr, 1)) {
            if (my_itr == _buffer.end()) {
                return false;
            }
            *my_itr = *in_itr;
        }
        return true;
    }

    auto get_max_pwm() -> uint16_t { return _max_pwm; }

    auto buffer() -> InBuffer& { return _buffer; }

    auto active() -> bool { return _active; }

  private:
    const uint16_t _max_pwm;
    InBuffer _buffer;
    InBuffer::iterator _input_buffer_begin;
    InBuffer::iterator _input_buffer_end;
    bool _active;
};

SCENARIO("xt1511 driver works") {
    GIVEN("an XT1511String with 16 pixels and 16-bit PWM values") {
        constexpr size_t led_count = 16;
        constexpr uint16_t max_pwm = 1000;
        XT1511String<uint16_t, led_count> leds;
        constexpr uint16_t off_value = leds.PWM_OFF_PERCENTAGE * max_pwm;
        constexpr uint16_t on_value = leds.PWM_ON_PERCENTAGE * max_pwm;
        constexpr size_t pwm_count = 32 * 16;  // Does not include stop bit
        auto policy = TestXT1511Policy<led_count>(max_pwm);
        WHEN("writing the default pixels") {
            REQUIRE(leds.write(policy));
            THEN("the output is 32*16 0's and then stop bits") {
                for (size_t i = 0; i < pwm_count; ++i) {
                    REQUIRE(policy.buffer().at(i) == off_value);
                }
                REQUIRE(policy.buffer().at(pwm_count) == leds.PWM_STOP_VALUE);
            }
            THEN(
                "the end_send command has been called at end of transmission") {
                REQUIRE(!policy.active());
            }
        }
        WHEN("updating the first pixel") {
            auto pixel = XT1511{.g = 0x1, .r = 0x2, .b = 0x4, .w = 0x8};
            leds.pixel(0) = pixel;
            auto set_indices = std::array{7, 14, 21, 28};
            THEN("writing out the pixels") {
                REQUIRE(leds.write(policy));
                THEN("data is correct") {
                    for (size_t i = 0; i < pwm_count; ++i) {
                        if (std::find(set_indices.begin(), set_indices.end(),
                                      i) != set_indices.end()) {
                            REQUIRE(policy.buffer().at(i) == on_value);
                        } else {
                            REQUIRE(policy.buffer().at(i) == off_value);
                        }
                    }
                    REQUIRE(policy.buffer().at(pwm_count) ==
                            leds.PWM_STOP_VALUE);
                }
            }
            THEN("setting all the pixels to fully on") {
                pixel.g = 0xFF;
                pixel.r = 0xFF;
                pixel.b = 0xFF;
                pixel.w = 0xFF;
                leds.set_all(pixel);
                THEN("writing out the pixels") {
                    REQUIRE(leds.write(policy));
                    THEN("data is correct") {
                        for (size_t i = 0; i < pwm_count; ++i) {
                            REQUIRE(policy.buffer().at(i) == on_value);
                        }
                        REQUIRE(policy.buffer().at(pwm_count) ==
                                leds.PWM_STOP_VALUE);
                    }
                }
            }
        }
    }
}
