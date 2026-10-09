#include "firmware/serial_stream.hpp"

#include <iostream>
#include <streambuf>

#include "firmware/serial_hardware.h"

namespace {
class SerialStreamBuffer final : public std::streambuf {
  protected:
    auto overflow(int_type character) -> int_type override {
        if (traits_type::eq_int_type(character, traits_type::eof())) {
            return traits_type::not_eof(character);
        }
        char byte = traits_type::to_char_type(character);
        return serial_hardware_write(&byte, 1) ? character : traits_type::eof();
    }

    auto xsputn(const char* data, std::streamsize length)
        -> std::streamsize override {
        if (length <= 0) {
            return 0;
        }
        return serial_hardware_write(data, static_cast<size_t>(length)) ? length
                                                                        : 0;
    }
};

SerialStreamBuffer serial_stream_buffer;
}  // namespace

auto serial_stream_redirect() -> void {
    std::cout.rdbuf(&serial_stream_buffer);
    std::cerr.rdbuf(&serial_stream_buffer);
}