#include "net/tcp_client.hpp"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using ssize_t = std::intptr_t;
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace minerby {
namespace {

#ifdef _WIN32
constexpr TcpClient::socket_t kInvalid = static_cast<TcpClient::socket_t>(INVALID_SOCKET);

struct WsaInit {
  WsaInit() {
    WSADATA d;
    WSAStartup(MAKEWORD(2, 2), &d);
  }
  ~WsaInit() { WSACleanup(); }
};
void ensure_wsa() { static WsaInit init; }

void set_recv_timeout(TcpClient::socket_t s, int ms) {
  DWORD tv = static_cast<DWORD>(ms);
  ::setsockopt(static_cast<SOCKET>(s), SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));
}
void close_sock(TcpClient::socket_t s) { ::closesocket(static_cast<SOCKET>(s)); }
#else
constexpr TcpClient::socket_t kInvalid = static_cast<TcpClient::socket_t>(-1);
void ensure_wsa() {}
void set_recv_timeout(TcpClient::socket_t s, int ms) {
  timeval tv;
  tv.tv_sec = ms / 1000;
  tv.tv_usec = (ms % 1000) * 1000;
  ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}
void close_sock(TcpClient::socket_t s) { ::close(static_cast<int>(s)); }
#endif

}  // namespace

TcpClient::TcpClient() : sock_(kInvalid) { ensure_wsa(); }
TcpClient::~TcpClient() { close(); }

bool TcpClient::is_open() const { return sock_ != kInvalid; }

void TcpClient::close() {
  if (sock_ != kInvalid) {
    close_sock(sock_);
    sock_ = kInvalid;
  }
  rx_.clear();
}

bool TcpClient::connect(const std::string& host, uint16_t port, int timeout_ms) {
  close();

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* res = nullptr;
  const std::string port_s = std::to_string(port);
  if (::getaddrinfo(host.c_str(), port_s.c_str(), &hints, &res) != 0 || !res) {
    return false;
  }

  socket_t s = kInvalid;
  for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
    s = static_cast<socket_t>(::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));
    if (s == kInvalid) continue;
#ifdef _WIN32
    if (::connect(static_cast<SOCKET>(s), ai->ai_addr,
                  static_cast<int>(ai->ai_addrlen)) == 0) {
      break;
    }
#else
    if (::connect(static_cast<int>(s), ai->ai_addr, ai->ai_addrlen) == 0) break;
#endif
    close_sock(s);
    s = kInvalid;
  }
  ::freeaddrinfo(res);

  if (s == kInvalid) return false;

  set_recv_timeout(s, timeout_ms);
  sock_ = s;
  return true;
}

bool TcpClient::send_all(const std::string& data) {
  if (sock_ == kInvalid) return false;
  const char* p = data.data();
  std::size_t left = data.size();
  while (left > 0) {
#ifdef _WIN32
    int n = ::send(static_cast<SOCKET>(sock_), p, static_cast<int>(left), 0);
#else
    ssize_t n = ::send(static_cast<int>(sock_), p, left, 0);
#endif
    if (n <= 0) return false;
    p += n;
    left -= static_cast<std::size_t>(n);
  }
  return true;
}

bool TcpClient::read_line(std::string& out, int timeout_ms) {
  if (sock_ == kInvalid) return false;
  set_recv_timeout(sock_, timeout_ms);

  for (;;) {
    std::size_t nl = rx_.find('\n');
    if (nl != std::string::npos) {
      out = rx_.substr(0, nl);
      if (!out.empty() && out.back() == '\r') out.pop_back();
      rx_.erase(0, nl + 1);
      return true;
    }

    char buf[4096];
#ifdef _WIN32
    int n = ::recv(static_cast<SOCKET>(sock_), buf, static_cast<int>(sizeof(buf)), 0);
#else
    ssize_t n = ::recv(static_cast<int>(sock_), buf, sizeof(buf), 0);
#endif
    if (n <= 0) return false;  // EOF, timeout, or error
    rx_.append(buf, static_cast<std::size_t>(n));
    if (rx_.size() > (1u << 20)) return false;  // runaway line guard
  }
}

}  // namespace minerby
