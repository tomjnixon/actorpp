#include "actorpp/actor.hpp"
#include "catch2/catch.hpp"
#include <iostream>
using namespace std::chrono_literals;

using namespace actorpp;

class PingPong : public Actor {
public:
  PingPong(Channel<int> pong) : ping(*this), exit(*this), pong(pong) {}
  Channel<int> ping;
  Channel<bool> exit;

  void run() {
    while (true) {
      switch (wait(ping, exit)) {
      case 0:
        pong.push(ping.pop());
        break;
      case 1:
        if (exit.pop())
          return;
        break;
      }
    }
  }

  Channel<int> pong;
};

TEST_CASE("ping pong") {
  Actor self;
  Channel<int> pong(self);
  ActorThread<PingPong> pp(pong);

  pp.ping.push(5);
  self.wait(pong);
  REQUIRE(pong.pop() == 5);
  pp.ping.push(6);
  self.wait(pong);
  REQUIRE(pong.pop() == 6);

  pp.exit.push(true);
}

TEST_CASE("ping pong no self") {
  Channel<int> pong;
  ActorThread<PingPong> pp(pong);

  pp.ping.push(5);
  REQUIRE(pong.read() == 5);

  pp.ping.push(6);
  REQUIRE(pong.read() == 6);

  pp.exit.push(true);
}

template <typename TimeT> class SendAfter : public Actor {
public:
  SendAfter(Channel<bool> chan, const TimeT &wait_time)
      : chan(chan), wait_time(wait_time) {}
  Channel<bool> chan;
  TimeT wait_time;

  void run() {
    std::this_thread::sleep_for(wait_time);
    chan.push(true);
  }
};

TEST_CASE("wait_for") {
  Actor self;
  Channel<bool> chan(self);
  ActorThread<SendAfter<std::chrono::milliseconds>> pp(chan, 500ms);
  REQUIRE(self.wait_for(250ms, chan) == -1);
  REQUIRE(!chan.readable());
  REQUIRE(self.wait_for(1s, chan) == 0);
}

TEST_CASE("wait_until") {
  Actor self;
  Channel<bool> chan(self);
  ActorThread<SendAfter<std::chrono::milliseconds>> pp(chan, 500ms);
  auto start = std::chrono::steady_clock::now();
  REQUIRE(self.wait_until(start + 250ms, chan) == -1);
  REQUIRE(!chan.readable());
  REQUIRE(self.wait_until(start + 1s, chan) == 0);
}
