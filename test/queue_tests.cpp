// detailed tests of the queue behaviour in Channel/ChannelStore

#include "actorpp/actor.hpp"
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <queue>
using namespace std::chrono_literals;

using namespace actorpp;

struct Counters {
  int num_copies = 0;
  int num_moves = 0;
  int num_destruct = 0;
};

template <bool noexcept_move = true> class TestType {
  Counters &counters;

public:
  TestType(Counters &counters) : counters(counters) {}

  TestType(const TestType &other) : counters(other.counters) {
    counters.num_copies++;
  }

  TestType(TestType &&other) noexcept(noexcept_move)
      : counters(other.counters) {
    counters.num_moves++;
  }

  ~TestType() { counters.num_destruct++; }

  TestType &operator=(const TestType &other) {
    counters = other.counters;
    counters.num_copies++;
    return *this;
  }

  TestType &operator=(TestType &&other) noexcept(noexcept_move) {
    counters = other.counters;
    counters.num_moves++;
    return *this;
  }
};

using TestTypeMove = TestType<true>;
using TestTypeCopy = TestType<false>;

TEST_CASE("push_copy") {
  Channel<TestTypeMove> channel;

  Counters c;
  TestTypeMove tt(c);
  channel.push(tt);

  REQUIRE(channel.size() == 1);
  REQUIRE(c.num_copies == 1);
  REQUIRE(c.num_moves == 0);
  REQUIRE(c.num_destruct == 0);
}

TEST_CASE("push_move") {
  Channel<TestTypeMove> channel;

  Counters c;
  TestTypeMove tt(c);
  channel.push(std::move(tt));

  REQUIRE(channel.size() == 1);
  REQUIRE(c.num_copies == 0);
  REQUIRE(c.num_moves == 1);
  REQUIRE(c.num_destruct == 0);
}

TEST_CASE("push_two") {
  Channel<TestTypeMove> channel;

  Counters c1;
  TestTypeMove tt1(c1);
  channel.push(tt1);

  Counters c2;
  TestTypeMove tt2(c2);
  channel.push(tt2);

  REQUIRE(channel.size() == 2);

  // first is moved in reallocation
  REQUIRE(c1.num_copies == 1);
  REQUIRE(c1.num_moves == 1);
  REQUIRE(c1.num_destruct == 1);

  REQUIRE(c2.num_copies == 1);
  REQUIRE(c2.num_moves == 0);
  REQUIRE(c2.num_destruct == 0);
}

TEST_CASE("push_two_copy") {
  Channel<TestTypeCopy> channel;

  Counters c1;
  TestTypeCopy tt1(c1);
  channel.push(tt1);

  Counters c2;
  TestTypeCopy tt2(c2);
  channel.push(tt2);

  REQUIRE(channel.size() == 2);

  // first is copied in reallocation
  REQUIRE(c1.num_copies == 2);
  REQUIRE(c1.num_moves == 0);
  REQUIRE(c1.num_destruct == 1);

  REQUIRE(c2.num_copies == 1);
  REQUIRE(c2.num_moves == 0);
  REQUIRE(c2.num_destruct == 0);
}

TEST_CASE("push_pop") {
  Channel<TestTypeMove> channel;

  Counters c;
  TestTypeMove tt(c);
  channel.push(tt);

  REQUIRE(channel.size() == 1);
  REQUIRE(c.num_copies == 1);
  REQUIRE(c.num_moves == 0);
  REQUIRE(c.num_destruct == 0);

  TestTypeMove out = channel.pop();

  REQUIRE(channel.size() == 0);
  REQUIRE(c.num_copies == 1);
  REQUIRE(c.num_moves == 1);
  REQUIRE(c.num_destruct == 1);
}

TEST_CASE("destruct") {
  Counters c;
  TestTypeMove tt(c);

  {
    Channel<TestTypeMove> channel;
    channel.push(tt);
  }

  REQUIRE(c.num_copies == 1);
  REQUIRE(c.num_moves == 0);
  REQUIRE(c.num_destruct == 1);
}

template <typename T, bool noexcept_move = true> struct NonTrivial {
  using This = NonTrivial<T, noexcept_move>;

  T value;

  NonTrivial(T value) : value(value) {}

  NonTrivial(const This &other) = default;

  NonTrivial(This &&other) noexcept(noexcept_move) : value(other.value) {}

  This &operator=(const This &other) = default;

  This &operator=(This &&other) noexcept(noexcept_move) {
    value = other.value;
    return *this;
  }

  bool operator==(This &other) { return value == other.value; }
};

// check for coverage of cases in Channel<T>::resize
static_assert(std::is_trivially_copyable_v<int>);
static_assert(!std::is_trivially_copyable_v<NonTrivial<int, true>>);
static_assert(std::is_nothrow_move_constructible_v<NonTrivial<int, true>>);
static_assert(!std::is_trivially_copyable_v<NonTrivial<int, false>>);
static_assert(!std::is_nothrow_move_constructible_v<NonTrivial<int, false>>);

TEMPLATE_TEST_CASE("matches_std::queue", "", int, (NonTrivial<int, true>),
                   (NonTrivial<int, false>)) {
  Channel<TestType> channel;
  std::queue<TestType> stl_q;
  int next = 0;

  auto push = [&]() {
    int value = next++;
    channel.push(value);
    stl_q.push(value);
    REQUIRE(channel.size() == stl_q.size());
  };

  auto pop = [&]() {
    REQUIRE(stl_q.size());

    TestType value_q = channel.pop();

    TestType value_stl_q = stl_q.front();
    stl_q.pop();

    REQUIRE(value_q == value_stl_q);
    REQUIRE(channel.size() == stl_q.size());
  };

  SECTION("push_pop_4_single") {
    for (int i = 0; i < 4; i++)
      push();
    for (int i = 0; i < 4; i++)
      pop();

    for (int i = 0; i < 8; i++) {
      push();
      pop();
    }
  }

  SECTION("push_pop_3_single") {
    for (int i = 0; i < 3; i++)
      push();
    for (int i = 0; i < 3; i++)
      pop();

    for (int i = 0; i < 8; i++) {
      push();
      pop();
    }
  }

  SECTION("reallocate_wrap") {
    for (int i = 0; i < 4; i++)
      push();
    for (int i = 0; i < 2; i++)
      pop();
    for (int i = 0; i < 2; i++)
      push();
    // full with start at 2, this should reallocate
    push();

    for (int i = 0; i < 5; i++) {
      pop();
    }
  }
}

/// objects which optionally throw when they are copied
struct CopyThrows {
  int id;
  bool *copy_throws;

  CopyThrows(int id, bool *copy_throws = nullptr)
      : id(id), copy_throws(copy_throws) {}

  CopyThrows(const CopyThrows &other)
      : id(other.id), copy_throws(other.copy_throws) {
    if (copy_throws != nullptr && *copy_throws)
      throw std::runtime_error("test");
  }
};

static_assert(!std::is_trivially_copyable_v<CopyThrows>);
static_assert(!std::is_nothrow_move_constructible_v<CopyThrows>);

TEST_CASE("resize_except") {
  Channel<CopyThrows> channel;
  bool copy_throws = false;

  // third item throws when resizing from 4 to 8
  for (int i = 0; i < 4; i++)
    channel.emplace(i, i == 2 ? &copy_throws : nullptr);

  copy_throws = true;
  try {
    channel.emplace(4);
  } catch (const std::runtime_error &) {
  }
  copy_throws = false;

  REQUIRE(channel.size() == 4);
  for (int i = 0; i < 4; i++) {
    REQUIRE(channel.pop().id == i);
  }
  REQUIRE(channel.size() == 0);
}

TEST_CASE("push_except") {
  Channel<CopyThrows> channel;

  for (int i = 0; i < 3; i++)
    channel.emplace(i);

  bool copy_throws = true;
  CopyThrows ct(0, &copy_throws);
  try {
    channel.push(ct);
  } catch (const std::runtime_error &) {
  }

  REQUIRE(channel.size() == 3);
  for (int i = 0; i < 3; i++) {
    REQUIRE(channel.pop().id == i);
  }
  REQUIRE(channel.size() == 0);
}
