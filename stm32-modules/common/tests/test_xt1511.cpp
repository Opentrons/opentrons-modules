#include <cstdbool>
#include <iostream>

#include "catch2/catch.hpp"
#include "core/xt1511.hpp"

using namespace xt1511;

template <size_t N>
class TestXT1511Policy {
  public:
    static constexpr size_t BITS_PER_PIXEL = 24;
    using InBuffer = std::array<uint16_t, BITS_PER_PIXEL * 2>;
    using FullBuffer = std::array<uint16_t, (N + 1) * BITS_PER_PIXEL>;

    TestXT1511Policy() = delete;

    TestXT1511Policy(uint16_t max_pwm)
        : _max_pwm(max_pwm),
          _buffer{0},
          _buf_position(),
          _input_buffer_begin(),
          _input_buffer_end(),
          _in_position() {}

    auto start_send(InBuffer& buffer) -> bool {
        _input_buffer_begin = buffer.begin();
        _input_buffer_end = buffer.end();
        _in_position = buffer.begin();
        _buf_position = _buffer.begin();
        return true;
    }

    auto end_send() -> void { *_buf_position = 0; }

    // Simulate end of a transfer by copying all data
    auto wait_for_interrupt(uint32_t timeout) -> bool {
        for (size_t i = 0; i < BITS_PER_PIXEL; ++i) {
            if (_buf_position == _buffer.end()) {
                return false;
            }
            *_buf_position = *_in_position;
            std::advance(_in_position, 1);
            if (_in_position == _input_buffer_end) {
                _in_position = _input_buffer_begin;
            }
            std::advance(_buf_position, 1);
        }
        return true;
    }

    auto get_max_pwm() -> uint16_t { return _max_pwm; }

    auto buffer() -> FullBuffer& { return _buffer; }

  private:
    uint16_t _max_pwm;
    FullBuffer _buffer;
    FullBuffer::iterator _buf_position;
    InBuffer::iterator _input_buffer_begin;
    InBuffer::iterator _input_buffer_end;
    InBuffer::iterator _in_position;
};

SCENARIO("xt1511 driver works") {
    GIVEN("a XT1511String with 16 pixels and 16-bit PWM values") {
        constexpr size_t led_count = 16;
        constexpr uint16_t max_pwm = 1000;
        XT1511String<uint16_t, led_count> leds;
        constexpr uint16_t off_value = leds.PWM_OFF_PERCENTAGE * max_pwm;
        constexpr uint16_t on_value = leds.PWM_ON_PERCENTAGE * max_pwm;
        auto policy = TestXT1511Policy<led_count>(max_pwm);
        WHEN("writing the default pixels") {
            REQUIRE(leds.write(policy));
            THEN("the output is 24*16 0's and then stop bits") {
                constexpr size_t count = (24 * 16);
                for (size_t i = 0; i < count; ++i) {
                    REQUIRE(policy.buffer().at(i) == off_value);
                }
                REQUIRE(policy.buffer().at(count) == leds.PWM_STOP_VALUE);
            }
        }
        WHEN("updating the first pixel") {
            auto pixel = XT1511{.g = 0x1, .r = 0x2, .b = 0x4};
            leds.pixel(0) = pixel;
            auto set_indices = std::array{7, 14, 21};
            THEN("writing out the pixels") {
                REQUIRE(leds.write(policy));
                THEN("data is correct") {
                    constexpr size_t count = (24 * 16);
                    for (size_t i = 0; i < count; ++i) {
                        if (std::find(set_indices.begin(), set_indices.end(),
                                      i) != set_indices.end()) {
                            REQUIRE(policy.buffer().at(i) == on_value);
                        } else {
                            REQUIRE(policy.buffer().at(i) == off_value);
                        }
                    }
                    REQUIRE(policy.buffer().at(count) == leds.PWM_STOP_VALUE);
                }
            }
            THEN("setting all the pixels to fully on") {
                pixel.g = 0xFF;
                pixel.r = 0xFF;
                pixel.b = 0xFF;
                leds.set_all(pixel);
                THEN("writing out the pixels") {
                    REQUIRE(leds.write(policy));
                    THEN("data is correct") {
                        constexpr size_t count = (24 * 16);
                        for (size_t i = 0; i < count; ++i) {
                            REQUIRE(policy.buffer().at(i) == on_value);
                        }
                        REQUIRE(policy.buffer().at(count) ==
                                leds.PWM_STOP_VALUE);
                    }
                }
            }
        }
    }
}
