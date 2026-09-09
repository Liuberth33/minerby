#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include "pow/sha256.hpp"
#include "util/hex.hpp"

using namespace minerby;

namespace {
std::string sha_hex(const std::string& s) {
  uint8_t out[32];
  sha256(reinterpret_cast<const uint8_t*>(s.data()), s.size(), out);
  return to_hex(out, 32);
}
std::string shad_hex(const std::string& s) {
  uint8_t out[32];
  sha256d(reinterpret_cast<const uint8_t*>(s.data()), s.size(), out);
  return to_hex(out, 32);
}
}  // namespace

TEST_CASE("sha256 known-answer vectors") {
  CHECK(sha_hex("") ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(sha_hex("abc") ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(sha_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("sha256 streaming in chunks matches one-shot (multi-block)") {
  const std::string big(1000, 'a');
  uint8_t one_shot[32];
  sha256(reinterpret_cast<const uint8_t*>(big.data()), big.size(), one_shot);

  Sha256 h;
  std::size_t off = 0;
  const std::array<std::size_t, 5> chunks{1, 7, 64, 100, 250};
  for (std::size_t chunk : chunks) {
    const std::size_t n = std::min(chunk, big.size() - off);
    h.update(reinterpret_cast<const uint8_t*>(big.data()) + off, n);
    off += n;
  }
  h.update(reinterpret_cast<const uint8_t*>(big.data()) + off, big.size() - off);
  uint8_t streamed[32];
  h.finish(streamed);

  CHECK(to_hex(one_shot, 32) == to_hex(streamed, 32));
}

TEST_CASE("sha256d is sha256 applied twice") {
  uint8_t once[32], twice[32];
  const std::string msg = "minerby";
  sha256(reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), once);
  sha256(once, 32, twice);
  CHECK(shad_hex(msg) == to_hex(twice, 32));
}

TEST_CASE("sha256d known-answer for empty input") {
  CHECK(shad_hex("") ==
        "5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456");
}
