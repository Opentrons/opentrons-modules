#pragma once

#include "core/xt1511.hpp"

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
