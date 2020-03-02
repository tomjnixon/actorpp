// run `python test/test_server.py` when running these tests!

#include "actorpp/actor.hpp"
#include "actorpp/net.hpp"
#include "catch2/catch.hpp"

using namespace actorpp;

TEST_CASE("ping pong") {
  int fd = connect("localhost", 5001);
  {
    Actor self;
    Channel<std::vector<uint8_t>> on_message(self);
    Channel<CloseReason> on_close(self);

    ActorThread<RecvThread> recv(fd, on_message, on_close);

    send(fd, "ping", 4, MSG_NOSIGNAL);

    REQUIRE(self.wait(on_message, on_close) == 0);
    std::vector<uint8_t> buf = on_message.pop();
    REQUIRE(std::string((char *)buf.data(), buf.size()) == "pong");

    recv.exit();
  }
  close(fd);
}

TEST_CASE("exit") {
  int fd = connect("localhost", 5001);

  {
    Actor self;
    Channel<std::vector<uint8_t>> on_message(self);
    Channel<CloseReason> on_close(self);

    ActorThread<RecvThread> recv(fd, on_message, on_close);

    send(fd, "exit", 4, MSG_NOSIGNAL);

    REQUIRE(self.wait(on_message, on_close) == 1);
    REQUIRE(on_close.pop() == CloseReason::Normal);

    recv.exit();
  }

  close(fd);
}
