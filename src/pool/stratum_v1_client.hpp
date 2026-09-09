#pragma once

#include <cstdint>
#include <string>

#include "net/tcp_client.hpp"
#include "pool/pool_client.hpp"
#include "stratum/job.hpp"

namespace minerby {

// Bitcoin-style Stratum V1: subscribe / authorize / notify / set_difficulty /
// submit. Builds an 80-byte-header MiningJob for each notify. Single-threaded;
// drive it with poll().
class StratumV1Client final : public PoolClient {
 public:
  StratumV1Client(std::string host, uint16_t port, std::string user,
                  std::string pass);

  void set_callbacks(Callbacks cb) override { cb_ = std::move(cb); }
  bool connect() override;
  void disconnect() override;
  bool connected() const override { return conn_.is_open(); }
  bool poll(int timeout_ms) override;
  bool submit(const MiningJob& job, uint32_t nonce, const uint8_t digest[32]) override;
  double difficulty() const override { return difficulty_; }

 private:
  bool send_json(const std::string& line);
  void handle_line(const std::string& line);
  void rebuild_job();

  std::string host_;
  uint16_t port_;
  std::string user_;
  std::string pass_;

  TcpClient conn_;
  Callbacks cb_;
  int next_id_ = 1;
  int subscribe_id_ = -1;
  int authorize_id_ = -1;
  int last_submit_id_ = -1;

  std::string extranonce1_;
  int extranonce2_size_ = 4;
  double difficulty_ = 1.0;

  StratumJob job_;
  bool have_job_ = false;
  uint64_t xn2_counter_ = 0;
};

}  // namespace minerby
