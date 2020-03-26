#pragma once
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace actorpp {

template <typename T> class Channel;

namespace detail {
template <typename T> int readable_channel(int i, Channel<T> &c) {
  if (c.readable_with_lock())
    return i;
  else
    return -1;
}

template <typename T, typename... Ttail>
int readable_channel(int i, Channel<T> &c, Channel<Ttail> &... chans) {
  if (c.readable_with_lock())
    return i;
  else
    return readable_channel(i + 1, chans...);
}

struct ActorImpl {
  std::mutex mut;
  std::condition_variable cv;

  template <typename... T> int wait(Channel<T> &... c) {
    std::unique_lock<std::mutex> lock(mut);
    int i;
    while ((i = detail::readable_channel(0, c...)) == -1) {
      cv.wait(lock);
    }
    return i;
  }
};

template <typename T> struct ChannelImpl {
  ChannelImpl(std::shared_ptr<ActorImpl> actor_impl)
      : actor_impl(std::move(actor_impl)) {}
  std::shared_ptr<detail::ActorImpl> actor_impl;
  std::queue<T> elements;

  template <typename TT> void push(TT &&item) {
    std::unique_lock<std::mutex> lock(actor_impl->mut);
    elements.push(std::forward<TT>(item));
    actor_impl->cv.notify_one();
  }

  template <class... Args> void emplace(Args &&... args) {
    std::unique_lock<std::mutex> lock(actor_impl->mut);
    elements.emplace(std::forward<Args>(args)...);
    actor_impl->cv.notify_one();
  }

  T pop() {
    std::unique_lock<std::mutex> lock(actor_impl->mut);
    T element = std::move(elements.front());
    assert(readable_with_lock());
    elements.pop();
    return element;
  }

  T read() {
    std::unique_lock<std::mutex> lock(actor_impl->mut);
    actor_impl->cv.wait(lock, [&] { return readable_with_lock(); });
    T element = std::move(elements.front());
    elements.pop();
    return element;
  }

  bool readable() {
    std::unique_lock<std::mutex> lock(actor_impl->mut);
    return readable_with_lock();
  }

  bool readable_with_lock() { return !elements.empty(); }
};
} // namespace detail

/// An actor, whose only ability is to wait for data in associated channels. To
/// run an actor in another thread, see ActorThread
class Actor {
public:
  std::shared_ptr<detail::ActorImpl> impl;

  Actor() : impl(std::make_shared<detail::ActorImpl>()) {}

  /// Wait for data to arrive in one of n channels; returns the index of the
  /// first channel that has available data. All channels be associated with
  /// this actor.
  template <typename... T> int wait(Channel<T> &... c) {
    return impl->wait(c...);
  }
};

/// A typed channel with an unbounded number of entries
template <typename T> class Channel {
  std::shared_ptr<detail::ChannelImpl<T>> impl;

public:
  using type = T;
  /// associated with a specified actor, which allows that actor to wait for
  /// this channel at the same time as others.
  Channel(Actor &actor)
      : impl(std::make_shared<detail::ChannelImpl<T>>(actor.impl)) {}
  /// not associated with any actor
  Channel()
      : impl(std::make_shared<detail::ChannelImpl<T>>(
            std::make_shared<detail::ActorImpl>())) {}

  template <typename TT> void push(TT &&item) {
    impl->push(std::forward<TT>(item));
  }

  template <typename... Args> void emplace(Args... args) {
    impl->emplace(std::forward<Args>(args)...);
  }

  /// pop an element, will assert if empty
  T pop() { return impl->pop(); }

  /// pop an element, blocking if empty
  T read() { return impl->read(); }

  /// is this non-empty?
  bool readable() { return impl->readable(); }

  /// is this non-empty? requires the associated lock to be held
  bool readable_with_lock() { return impl->readable_with_lock(); }
};

/// Wrapper around a class derived from Actor, which runs its `void run()`
/// method in a thread.
///
/// This can't be implemented nicely through regular inheritance, because the
/// constructor of a base class can't safely call derived methods, so we can't
/// start the thread from the constructor.
template <typename ActorT> class ActorThread : public ActorT {
public:
  template <typename... Args>
  ActorThread(Args&&... args)
      : ActorT(std::forward<Args>(args)...),
        thread([&] { this->template run(); }) {}

  ~ActorThread() { thread.join(); }

private:
  std::thread thread;
};

} // namespace actorpp
