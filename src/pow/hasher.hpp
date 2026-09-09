#pragma once

#include <cstddef>
#include <cstdint>

namespace minerby {

// A PoW hashing kernel. Phase 1: Sha256dHasher. Phase 2: RandomXHasher.
class IHasher {
 public:
  virtual ~IHasher() = default;

  // Human-readable algorithm name, e.g. "sha256d".
  virtual const char* name() const = 0;

  // Hash `len` bytes at `data` and write a 32-byte digest to `out`.
  // The digest is compared against a little-endian target by the miner.
  virtual void hash(const uint8_t* data, std::size_t len, uint8_t out[32]) = 0;
};

}  // namespace minerby
