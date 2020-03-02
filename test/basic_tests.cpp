#include "actorpp/actor.hpp"
#include "catch2/catch.hpp"
#include <iostream>

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
