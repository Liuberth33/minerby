#pragma once

#include "pow/hasher.hpp"

namespace minerby {

class Sha256dHasher final : public IHasher {
 public:
  const char* name() const override { return "sha256d"; }
  void hash(const uint8_t* data, std::size_t len, uint8_t out[32]) override;
};

}  // namespace minerby
