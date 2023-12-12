#pragma once
#include "actor.hpp"
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

namespace actorpp {

enum class CloseReason {
  Normal,
  Error,
};

#if !defined(ACTORPP_RECV_THREAD_SHUTDOWN) && !defined(ACTORPP_RECV_THREAD_PIPE)
#define ACTORPP_RECV_THREAD_PIPE
#endif

#if defined(ACTORPP_RECV_THREAD_SHUTDOWN)

class RecvThread : Actor {
public:
  RecvThread(int fd, Channel<std::vector<uint8_t>> on_message,
             Channel<CloseReason> on_close)
      : fd(fd), on_message(std::move(on_message)),
        on_close(std::move(on_close)) {}
  void run() {
    while (true) {
      std::vector<uint8_t> buf(128);
      int bytes_read = recv(fd, buf.data(), buf.size(), 0);
      if (bytes_read > 0) {
        buf.resize(bytes_read);
        on_message.push(std::move(buf));
      } else {
        on_close.push(CloseReason::Normal);
        break;
      }
    }
  }

  void exit() { shutdown(fd, SHUT_RD); }

private:
  int fd;
  Channel<std::vector<uint8_t>> on_message;
  Channel<CloseReason> on_close;
};

#elif defined(ACTORPP_RECV_THREAD_PIPE)
#include <sys/poll.h>

class RecvThread : Actor {
public:
  RecvThread(int fd, Channel<std::vector<uint8_t>> on_message,
             Channel<CloseReason> on_close)
      : fd(fd), on_message(std::move(on_message)),
        on_close(std::move(on_close)) {
    if (pipe(pipe_fds) != 0)
      throw std::runtime_error("pipe() failed");
  }
  void run() {
    struct pollfd fds[2];
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    fds[1].fd = pipe_fds[0];
    fds[1].events = POLLIN;
    while (true) {
      if (poll(fds, 2, -1) <= 0)
        throw std::runtime_error("poll() failed");
      if (fds[1].revents != 0)
        break;
      if (fds[0].revents & POLLIN) {
        std::vector<uint8_t> buf(128);
        int bytes_read = recv(fd, buf.data(), buf.size(), 0);
        if (bytes_read > 0) {
          buf.resize(bytes_read);
          on_message.push(std::move(buf));
        } else {
          on_close.push(CloseReason::Normal);
          break;
        }
      }
    }

    // wait for data and close the read side of the pipe; this ensures that
    // exit() never tries to write into a closed pipe
    char buf[1];
    if (fds[1].revents == 0)
      if (read(pipe_fds[0], buf, 1) != 1)
        throw std::runtime_error("read(pipe fd 0) failed");

    if (close(pipe_fds[0]) != 0)
      throw std::runtime_error("close(pipe fd 0) failed");
  }

  void exit() {
    if (pipe_fds[1]) {
      if (write(pipe_fds[1], "q", 1) != 1)
        throw std::runtime_error("write(pipe fd 1) failed");
      if (close(pipe_fds[1]) != 0)
        throw std::runtime_error("close(pipe fd 1) failed");
      pipe_fds[1] = 0;
    }
  }

  ~RecvThread() { exit(); }

private:
  int fd;
  Channel<std::vector<uint8_t>> on_message;
  Channel<CloseReason> on_close;
  int pipe_fds[2];
};

#else
#error "unknown RecvThread implementation"
#endif

int connect(const std::string &hostname, int port) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res;

  std::string port_str = std::to_string(port);
  int err = getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &res);

  if (err != 0 || res == NULL) {
    throw std::runtime_error("dns lookup failed");
  }

  int sockfd = socket(res->ai_family, res->ai_socktype, 0);
  if (sockfd < 0) {
    freeaddrinfo(res);
    throw std::runtime_error("failed to allocate socket");
  }

  if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
    close(sockfd);
    freeaddrinfo(res);
    throw std::runtime_error("failed to connect");
  }

  freeaddrinfo(res);

  return sockfd;
}

} // namespace actorpp
