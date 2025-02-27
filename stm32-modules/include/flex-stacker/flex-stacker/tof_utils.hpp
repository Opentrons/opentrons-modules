#include <array>
#include <cstdint>

// start at -6 so that the first byte will add 8 bits to it.
constexpr int PROCESSED_BITS = -6;
// Bits in a byte
constexpr int ONE_BYTE = 8;
// mask to produce 1 base64 char
constexpr int BASE64_CHAR_MASK = 0x3F;

// The length in bytes of the encoded string
// Base64 encodes three bytes to four characters. Sometimes, padding is added
// in the form '=' characters. We also add one extra byte for the null
// terminator '\0' to denote the fixed char array as a string.
template <std::size_t N>
constexpr uint8_t BASE64_ENCODED_LEN = ((N * 4) / 3 + 3) + 1;

// Base64 encoding characters
static const char BASE64_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

template <std::size_t N>
inline auto base64_encode(const std::array<uint8_t, N>& data,
                          std::array<char, BASE64_ENCODED_LEN<N>>& encoded)
    -> bool {
    int idx = 0;
    int val = 0;
    int valb = PROCESSED_BITS;
    for (uint8_t c : data) {
        val = (val << ONE_BYTE) + c;
        valb += ONE_BYTE;
        while (valb >= 0) {
            // Mask & 0x3F (b111111) to extract the lowest 6 bits,
            // which correspond to a single Base64 character.
            encoded[idx] = BASE64_ALPHABET[(val >> valb) & BASE64_CHAR_MASK];
            valb += PROCESSED_BITS;
            idx += 1;
        }
    }
    if (valb > PROCESSED_BITS) {
        encoded[idx] =
            BASE64_ALPHABET[((val << ONE_BYTE) >> (valb + ONE_BYTE)) &
                            BASE64_CHAR_MASK];
        idx += 1;
    }
    // Base64 encoding always produces a multiple of 4 characters.
    // If the encoded string is not a multiple of 4, it pads the string
    // with '=' characters if there is enough space in the buffer.
    while (idx < BASE64_ENCODED_LEN<N> - 1 && encoded[idx] != '=') {
        encoded[idx] = '=';
        idx += 1;
    }
    // Add null terminator
    encoded[idx] = '\0';
    return true;
}
