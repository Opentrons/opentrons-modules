#pragma once

#include <iterator>
#include <ranges>
#include <span>
#include <type_traits>

namespace bit_utils {

template <typename Iter>
concept ByteIterator = requires {
    {std::forward_iterator<Iter>};
    {std::is_same_v<std::iter_value_t<Iter>, uint8_t>};
};

/**
 * Convert a span of bytes into a single integer. The first index being the most
 * significant byte. Big endian.
 * @tparam InputIter The input iterator to read from, which must refer to a
 * byte and satisfy InputIterator
 * @tparam InputEnd The limit to the input, which must satisfy
 * is_sentinel_for<InputIter>
 * @tparam OutputIntType The type of the result. Must be an integer.
 * @param input Input iterator to read from
 * @param limit Input limit to read until
 * @param output The output parameter.
 * @returns Iterator to one-past where the last byte was read from.
 *
 * If the input iterator pair does not contain enough data to completely specify
 * OutputType, then OutputType will remain incomplete in the less-significant
 * parts, e.g. trying to parse a uint32_t from 0x112233 will result in output =
 * 0x11223300.
 */
template <typename Input, typename Limit, typename OutputIntType>
requires std::is_integral_v<OutputIntType> && ByteIterator<Input> &&
    std::sentinel_for<Limit, Input>
[[nodiscard]] auto bytes_to_int(Input input, Limit limit, OutputIntType& output)
    -> Input {
    output = 0;
    for (ssize_t byte_index = sizeof(output) - 1;
         input != limit && byte_index >= 0; input++, byte_index--) {
        output |= (static_cast<OutputIntType>(*input) << (byte_index * 8));
    }
    return input;
}

/**
 * overload of bytes_to_int to support direct range or view calls
 * @tparam InputContainer A type satisfying forward_range containing uint8_t
 * @tparam OutputIntType The type of the int to return; must satisfy
 * is_integral_v
 * @param input The input container to read from
 * @param output The output int to write to
 * @returns Iterator at one past the last byte that was read.
 *
 * For more details see the overload taking iterators.
 */

template <typename InputContainer, typename OutputIntType>
requires std::is_integral_v<OutputIntType> &&
    std::ranges::forward_range<InputContainer> &&
    std::is_same_v<std::ranges::range_value_t<InputContainer>, uint8_t>
auto bytes_to_int(const InputContainer& input, OutputIntType& output)
    -> std::ranges::iterator_t<const InputContainer> {
    return bytes_to_int(input.begin(), input.end(), output);
}

/**
 * Write an integer into a container. Big Endian.
 * @tparam Output Output iterator type satisfying output_iterator
 * @tparam Limit Limit for writing. Must satisfy sentinel_for<Output>.
 * @tparam InputIntType The type written into the iterator. Must be an integer.
 * Must satisfy is_integral_v.
 * @param output iterator to write to
 * @param limit Limit to write to
 * @param input integer
 * @returns iterator one-past-end of written bytes
 *
 * Bytes will be written into the container until either the size of the integer
 * or the size of the container is reached, meaning that output may be partial.
 */
template <typename InputIntType, typename Output, typename Limit>
requires std::is_integral_v<InputIntType> &&
    std::output_iterator<Output, uint8_t> && ByteIterator<Output> &&
    std::sentinel_for<Limit, Output>
[[nodiscard]] auto int_to_bytes(InputIntType input, Output output, Limit limit)
    -> Output {
    for (ssize_t x = sizeof(input) - 1; x >= 0 && output != limit;
         x--, output++) {
        *output = (input >> (x * 8));
    }
    return output;
}

}  // namespace bit_utils
