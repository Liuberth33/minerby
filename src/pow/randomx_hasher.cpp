#include "pow/randomx_hasher.hpp"

#include <stdexcept>

namespace minerby {

RandomXHasher::RandomXHasher(RandomXContext& ctx) {
  if (!ctx.seeded()) {
    throw std::runtime_error("randomx: context must be seeded before creating a VM");
  }
  vm_ = randomx_create_vm(ctx.vm_flags(), ctx.cache_for_vm(), ctx.dataset_for_vm());
  if (!vm_) {
    throw std::runtime_error(
        "randomx: randomx_create_vm failed (unsupported flags or allocation)");
  }
}

RandomXHasher::~RandomXHasher() {
  if (vm_) randomx_destroy_vm(vm_);
}

void RandomXHasher::hash(const uint8_t* data, std::size_t len, uint8_t out[32]) {
  randomx_calculate_hash(vm_, data, len, out);
}

}  // namespace minerby
