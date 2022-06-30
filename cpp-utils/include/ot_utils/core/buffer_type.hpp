#pragma once

#include <array>
#include <cstdint>

namespace ot_utils {
namespace utils {

template <typename Buffer>
concept GenericByteBuffer = requires(Buffer buff) {
    {std::is_same_v<Buffer, std::array<uint8_t, sizeof(Buffer)>>};
};

}  // namespace utils
}  // namespace ot_utils
