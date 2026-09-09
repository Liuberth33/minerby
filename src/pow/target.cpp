#include "pow/target.hpp"

namespace minerby {

Hash256 target_from_difficulty(double diff) {
  Hash256 t{};
  t.fill(0);
  if (diff <= 0.0) {
    t.fill(0xff);
    return t;
  }

  // Port of cpuminer's diff_to_target: target words are little-endian ordered,
  // word k holds the low 32 bits of m, word k+1 the high 32 bits.
  double d = diff;
  int k = 6;
  for (; k > 0 && d > 1.0; --k) d /= 4294967296.0;

  const uint64_t m = static_cast<uint64_t>(4294901760.0 / d);  // 0xffff0000 / d
  if (m == 0 && k == 6) {
    t.fill(0xff);
    return t;
  }

  auto put_word = [&](int word, uint32_t v) {
    t[word * 4 + 0] = static_cast<uint8_t>(v);
    t[word * 4 + 1] = static_cast<uint8_t>(v >> 8);
    t[word * 4 + 2] = static_cast<uint8_t>(v >> 16);
    t[word * 4 + 3] = static_cast<uint8_t>(v >> 24);
  };
  put_word(k, static_cast<uint32_t>(m));
  if (k + 1 < 8) put_word(k + 1, static_cast<uint32_t>(m >> 32));
  return t;
}

Hash256 target_from_compact(uint32_t bits) {
  const uint32_t exp = bits >> 24;
  uint32_t mant = bits & 0x007fffff;

  std::array<uint8_t, 32> be{};
  be.fill(0);

  if (exp <= 3) {
    mant >>= 8 * (3 - exp);
    be[31] = static_cast<uint8_t>(mant);
    be[30] = static_cast<uint8_t>(mant >> 8);
    be[29] = static_cast<uint8_t>(mant >> 16);
  } else {
    const int shift = static_cast<int>(exp) - 3;  // bytes
    for (int i = 0; i < 3; ++i) {
      const int pos = 32 - shift - 3 + i;
      if (pos >= 0 && pos < 32) be[pos] = static_cast<uint8_t>(mant >> (16 - 8 * i));
    }
  }

  Hash256 le{};
  for (int i = 0; i < 32; ++i) le[i] = be[31 - i];
  return le;
}

bool meets_target(const Hash256& hash, const Hash256& target) {
  for (int i = 31; i >= 0; --i) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;  // exactly equal counts as a hit
}

}  // namespace minerby
