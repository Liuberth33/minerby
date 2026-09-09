#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "miner/job.hpp"

namespace minerby {

// Common surface for a mining-pool connection. Implementations:
//   - StratumV1Client : Bitcoin-style Stratum V1 (sha256d)
//   - MoneroClient     : Monero / xmrig JSON protocol (RandomX)
class PoolClient {
 public:
  struct Callbacks {
    // A new job. `seed_hash` is empty for sha256d; for RandomX it is the key the
    // caller must feed to RandomXContext::ensure_seed before hashing.
    std::function<void(const MiningJob& job, const std::vector<uint8_t>& seed_hash)>
        on_job;
    std::function<void(bool accepted, const std::string& error)> on_submit_result;
  };

  virtual ~PoolClient() = default;

  virtual void set_callbacks(Callbacks cb) = 0;

  // Connect + authenticate, blocking until the first job arrives. False on error.
  virtual bool connect() = 0;
  virtual void disconnect() = 0;
  virtual bool connected() const = 0;

  // Read and dispatch one message; false means the connection dropped.
  virtual bool poll(int timeout_ms) = 0;

  // Submit a winning nonce. `digest` is the 32-byte hash of the blob with that
  // nonce spliced in (required by Monero; ignored by Stratum V1).
  virtual bool submit(const MiningJob& job, uint32_t nonce,
                      const uint8_t digest[32]) = 0;

  // Current share difficulty, for display only.
  virtual double difficulty() const = 0;
};

}  // namespace minerby
