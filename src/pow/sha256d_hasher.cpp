#include "pow/sha256d_hasher.hpp"

#include "pow/sha256.hpp"

namespace minerby {

void Sha256dHasher::hash(const uint8_t* data, std::size_t len, uint8_t out[32]) {
  sha256d(data, len, out);
}

}  // namespace minerby
