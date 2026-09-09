#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "net/tcp_client.hpp"
#include "stratum/job.hpp"

namespace minerby {

// Stratum V1 client: subscribe / authorize / notify / set_difficulty / submit.
// Single-threaded; drive it by calling poll() in a loop.
class StratumClient {
 public:
  struct Callbacks {
    std::function<void(double)> on_difficulty;
    std::function<void(const StratumJob&)> on_job;
    std::function<void(bool accepted, const std::string& error)> on_submit_result;
  };

  StratumClient(std::string host, uint16_t port, std::string user, std::string pass);

  void set_callbacks(Callbacks cb) { cb_ = std::move(cb); }

  // Connect, mining.subscribe, mining.authorize. False on any failure.
  bool connect_and_subscribe();
  void disconnect();
  bool connected() const { return conn_.is_open(); }

  // Read and dispatch one message. False means the connection dropped.
  bool poll(int timeout_ms = 2000);

  // mining.submit. `nonce_hex` / `ntime_hex` / `extranonce2_hex` are sent as-is.
  bool submit(const std::string& job_id, const std::string& extranonce2_hex,
              const std::string& ntime_hex, const std::string& nonce_hex);

  const std::string& extranonce1() const { return extranonce1_; }
  int extranonce2_size() const { return extranonce2_size_; }
  double difficulty() const { return difficulty_; }

 private:
  bool send_json(const std::string& line);
  void handle_line(const std::string& line);

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
};

}  // namespace minerby
