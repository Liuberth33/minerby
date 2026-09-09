#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace minerby {

inline std::string to_hex(const uint8_t* p, std::size_t n) {
  static const char* d = "0123456789abcdef";
  std::string s;
  s.resize(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    s[2 * i]     = d[p[i] >> 4];
    s[2 * i + 1] = d[p[i] & 0x0f];
  }
  return s;
}

inline int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

inline std::vector<uint8_t> from_hex(const std::string& s) {
  if (s.size() % 2 != 0) throw std::runtime_error("from_hex: odd length");
  std::vector<uint8_t> out;
  out.reserve(s.size() / 2);
  for (std::size_t i = 0; i < s.size(); i += 2) {
    int hi = hex_val(s[i]);
    int lo = hex_val(s[i + 1]);
    if (hi < 0 || lo < 0) throw std::runtime_error("from_hex: bad digit");
    out.push_back(static_cast<uint8_t>((hi << 4) | lo));
  }
  return out;
}

// Interpret 8 hex chars as a big-endian uint32 (Stratum version/nbits/ntime).
inline uint32_t be32_from_hex(const std::string& s) {
  std::vector<uint8_t> b = from_hex(s);
  if (b.size() != 4) throw std::runtime_error("be32_from_hex: need exactly 4 bytes");
  return (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
         (static_cast<uint32_t>(b[2]) << 8) | static_cast<uint32_t>(b[3]);
}

}  // namespace minerby
