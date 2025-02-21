#include <array>
#include <cstdint>
#include <string>

constexpr uint8_t HISTOGRAM_LEN = 135;
// start at -6 so that the first byte will add 8 bits to it.
constexpr int PROCESSED_BITS = -6;
// Bits in a byte
constexpr int ONE_BYTE = 8;
// base64 encoding hhas to be a multiple of 4.
constexpr int PADDING_MULTIPLE = 4;
// mask to produce 1 base64 char
constexpr int BASE64_CHAR_MASK = 0x3F;

// Base64 encoding characters
static const std::string BASE64_ALPHABET =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline auto base64_encode(const std::array<uint8_t, HISTOGRAM_LEN>& data)
    -> std::string {
    std::string encoded;
    int val = 0;
    int valb = PROCESSED_BITS;
    for (uint8_t c : data) {
        val = (val << ONE_BYTE) + c;
        valb += ONE_BYTE;
        while (valb >= 0) {
            // we mask & 0x3F (b111111) to extract the lowest 6 bits,
            // which correspond to a single Base64 character.
            encoded.push_back(
                BASE64_ALPHABET[(val >> valb) & BASE64_CHAR_MASK]);
            valb += PROCESSED_BITS;
        }
    }
    if (valb > PROCESSED_BITS) {
        encoded.push_back(
            BASE64_ALPHABET[((val << ONE_BYTE) >> (valb + ONE_BYTE)) &
                            BASE64_CHAR_MASK]);
    }
    // Base64 encoding always produces a multiple of 4 characters. If the
    // encoded string is not a multiple of 4, it pads the string with =
    // characters.
    while (encoded.size() % PADDING_MULTIPLE == 0) {
        encoded.push_back('=');
    }
    return encoded;
}
