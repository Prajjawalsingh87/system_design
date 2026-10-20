// Base62 encoding of 64-bit IDs into short, URL-safe strings using [0-9a-zA-Z].
// 62^11 > 2^64, so any uint64 fits in at most 11 characters.
#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace base62 {

inline constexpr char kAlphabet[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
inline constexpr std::uint64_t kBase = 62;

inline std::string encode(std::uint64_t value) {
    if (value == 0) return "0";
    std::string out;
    while (value > 0) {
        out.push_back(kAlphabet[value % kBase]);
        value /= kBase;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

inline int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 36;
    return -1;
}

// Throws std::invalid_argument on an empty string or a bad character, and
// std::overflow_error if the value does not fit in 64 bits.
inline std::uint64_t decode(const std::string& text) {
    if (text.empty()) throw std::invalid_argument("empty base62 string");
    std::uint64_t value = 0;
    for (char c : text) {
        const int d = digit_value(c);
        if (d < 0) throw std::invalid_argument(std::string("invalid base62 character '") + c + "'");
        if (value > (UINT64_MAX - static_cast<std::uint64_t>(d)) / kBase)
            throw std::overflow_error("base62 value does not fit in 64 bits");
        value = value * kBase + static_cast<std::uint64_t>(d);
    }
    return value;
}

}  // namespace base62
