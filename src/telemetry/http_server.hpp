#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace minerby {

// Tiny single-threaded HTTP/1.1 endpoint. Serves:
//   GET /metrics  -> body_provider() as text/plain (Prometheus exposition)
//   GET /healthz  -> "ok"
// Anything else -> 404. Intended for localhost scraping, not the public net.
class HttpServer {
 public:
  HttpServer(uint16_t port, std::function<std::string()> body_provider);
  ~HttpServer();

  bool start();  // false if the port could not be bound
  void stop();

 private:
  void run();

  uint16_t port_;
  std::function<std::string()> body_provider_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::uintptr_t listen_sock_;
};

}  // namespace minerby
