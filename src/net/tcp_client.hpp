#pragma once

#include <cstdint>
#include <string>

namespace minerby {

// Minimal blocking TCP client with line-oriented reads, used for the
// newline-delimited JSON-RPC that Stratum speaks.
class TcpClient {
 public:
#ifdef _WIN32
  using socket_t = std::uintptr_t;
#else
  using socket_t = int;
#endif

  TcpClient();
  ~TcpClient();
  TcpClient(const TcpClient&) = delete;
  TcpClient& operator=(const TcpClient&) = delete;

  bool connect(const std::string& host, uint16_t port, int timeout_ms = 10000);
  void close();
  bool is_open() const;

  // Send the whole buffer. False on any error.
  bool send_all(const std::string& data);

  // Read one '\n'-terminated line (newline stripped, '\r' too).
  // False on EOF, socket error, or timeout.
  bool read_line(std::string& out, int timeout_ms = 60000);

 private:
  socket_t sock_;
  std::string rx_;
};

}  // namespace minerby
