#pragma once

#include <randomx.h>

#include "pow/hasher.hpp"
#include "pow/randomx_context.hpp"

namespace minerby {

// One RandomX VM, bound to the shared context's cache/dataset. Not copyable;
// each worker thread owns one.
class RandomXHasher final : public IHasher {
 public:
  explicit RandomXHasher(RandomXContext& ctx);
  ~RandomXHasher() override;
  RandomXHasher(const RandomXHasher&) = delete;
  RandomXHasher& operator=(const RandomXHasher&) = delete;

  const char* name() const override { return "randomx"; }
  void hash(const uint8_t* data, std::size_t len, uint8_t out[32]) override;

 private:
  randomx_vm* vm_ = nullptr;
};

}  // namespace minerby
