#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "net/tcp_client.hpp"
#include "pool/pool_client.hpp"

namespace minerby {

// Monero / xmrig JSON pool protocol: login / job / submit over a line-delimited
// TCP stream. Produces RandomX MiningJobs (blob + seed_hash, nonce at offset 39).
class MoneroClient final : public PoolClient {
 public:
  MoneroClient(std::string host, uint16_t port, std::string user, std::string pass);

  void set_callbacks(Callbacks cb) override { cb_ = std::move(cb); }
  bool connect() override;
  void disconnect() override;
  bool connected() const override { return conn_.is_open(); }
  bool poll(int timeout_ms) override;
  bool submit(const MiningJob& job, uint32_t nonce, const uint8_t digest[32]) override;
  double difficulty() const override { return difficulty_; }

 private:
  bool send_json(const std::string& line);
  void handle(const nlohmann::json& msg);
  bool apply_job(const nlohmann::json& j);
  void maybe_keepalive();

  std::string host_;
  uint16_t port_;
  std::string user_;
  std::string pass_;

  TcpClient conn_;
  Callbacks cb_;
  int next_id_ = 1;
  int login_id_ = -1;
  int last_submit_id_ = -1;

  std::string rpc_id_;   // session id returned by login, echoed on submit
  bool logged_in_ = false;
  double difficulty_ = 0.0;
  std::chrono::steady_clock::time_point last_keepalive_{};
};

}  // namespace minerby
