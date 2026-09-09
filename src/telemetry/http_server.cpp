#include "telemetry/http_server.hpp"

#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

namespace minerby {
namespace {

#ifdef _WIN32
using raw_sock = SOCKET;
constexpr std::uintptr_t kInvalid = static_cast<std::uintptr_t>(INVALID_SOCKET);
void close_sock(std::uintptr_t s) { ::closesocket(static_cast<raw_sock>(s)); }
int recv_some(std::uintptr_t s, char* b, int n) {
  return ::recv(static_cast<raw_sock>(s), b, n, 0);
}
void send_str(std::uintptr_t s, const std::string& d) {
  ::send(static_cast<raw_sock>(s), d.data(), static_cast<int>(d.size()), 0);
}
#else
using raw_sock = int;
constexpr std::uintptr_t kInvalid = static_cast<std::uintptr_t>(-1);
void close_sock(std::uintptr_t s) { ::close(static_cast<raw_sock>(s)); }
int recv_some(std::uintptr_t s, char* b, int n) {
  return static_cast<int>(::recv(static_cast<raw_sock>(s), b, static_cast<size_t>(n), 0));
}
void send_str(std::uintptr_t s, const std::string& d) {
  ::send(static_cast<raw_sock>(s), d.data(), d.size(), 0);
}
#endif

raw_sock rs(std::uintptr_t s) { return static_cast<raw_sock>(s); }

std::string http_response(int code, const std::string& status,
                          const std::string& content_type, const std::string& body) {
  return "HTTP/1.1 " + std::to_string(code) + " " + status + "\r\n" +
         "Content-Type: " + content_type + "\r\n" +
         "Content-Length: " + std::to_string(body.size()) + "\r\n" +
         "Connection: close\r\n\r\n" + body;
}

}  // namespace

HttpServer::HttpServer(uint16_t port, std::function<std::string()> body_provider)
    : port_(port),
      body_provider_(std::move(body_provider)),
      listen_sock_(kInvalid) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start() {
#ifdef _WIN32
  { WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa); }  // refcounted; matched at exit
#endif
  std::uintptr_t s = static_cast<std::uintptr_t>(::socket(AF_INET, SOCK_STREAM, 0));
  if (s == kInvalid) return false;

  int yes = 1;
  ::setsockopt(rs(s), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&yes), sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (::bind(rs(s), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
      ::listen(rs(s), 8) != 0) {
    close_sock(s);
    return false;
  }

  listen_sock_ = s;
  running_.store(true);
  thread_ = std::thread([this] { run(); });
  spdlog::info("metrics: http endpoint on http://127.0.0.1:{}/metrics", port_);
  return true;
}

void HttpServer::stop() {
  if (!running_.exchange(false)) return;
  if (listen_sock_ != kInvalid) {
    close_sock(listen_sock_);
    listen_sock_ = kInvalid;
  }
  if (thread_.joinable()) thread_.join();
}

void HttpServer::run() {
  while (running_.load()) {
    std::uintptr_t c = static_cast<std::uintptr_t>(::accept(rs(listen_sock_), nullptr, nullptr));
    if (c == kInvalid) {
      if (!running_.load()) break;
      continue;
    }

    char buf[2048];
    int n = recv_some(c, buf, sizeof(buf) - 1);
    std::string req = (n > 0) ? std::string(buf, static_cast<std::size_t>(n)) : std::string{};

    std::string resp;
    if (req.rfind("GET /metrics", 0) == 0) {
      resp = http_response(200, "OK", "text/plain; version=0.0.4", body_provider_());
    } else if (req.rfind("GET /healthz", 0) == 0) {
      resp = http_response(200, "OK", "text/plain", "ok");
    } else {
      resp = http_response(404, "Not Found", "text/plain", "not found\n");
    }
    send_str(c, resp);
    close_sock(c);
  }
}

}  // namespace minerby
