#pragma once

#include <cstddef>
#include <cstdint>

namespace minerby {

// Streaming SHA-256 (FIPS 180-4). Not optimised - Phase 1 uses this as the
// reference kernel; Phase 2 swaps in RandomX behind IHasher.
class Sha256 {
 public:
  Sha256() { reset(); }
  void reset();
  void update(const uint8_t* data, std::size_t len);
  void finish(uint8_t out[32]);

 private:
  void transform(const uint8_t block[64]);

  uint32_t state_[8];
  uint64_t bitlen_;
  uint8_t buf_[64];
  std::size_t buflen_;
};

void sha256(const uint8_t* data, std::size_t len, uint8_t out[32]);

// Bitcoin-style double SHA-256: sha256(sha256(x)).
void sha256d(const uint8_t* data, std::size_t len, uint8_t out[32]);

}  // namespace minerby
