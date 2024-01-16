#pragma once
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>

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
int readable_channel(int i, Channel<T> &c, Channel<Ttail> &...chans) {
  if (c.readable_with_lock())
    return i;
  else
    return readable_channel(i + 1, chans...);
}

struct ActorImpl {
  std::mutex mut;
  std::condition_variable cv;

  template <typename... T> int wait(Channel<T> &...c) {
    std::unique_lock<std::mutex> lock(mut);
    int i;
    cv.wait(lock,
            [&]() { return (i = detail::readable_channel(0, c...)) != -1; });
    return i;
  }

  template <class Clock, class Duration, typename... T>
  int wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time,
                 Channel<T> &...c) {
    std::unique_lock<std::mutex> lock(mut);
    int i;
    cv.wait_until(lock, timeout_time, [&]() {
      return (i = detail::readable_channel(0, c...)) != -1;
    });
    return i;
  }

  template <class Rep, class Period, typename... T>
  int wait_for(const std::chrono::duration<Rep, Period> &rel_time,
               Channel<T> &...c) {
    std::unique_lock<std::mutex> lock(mut);
    int i;
    cv.wait_for(lock, rel_time, [&]() {
      return (i = detail::readable_channel(0, c...)) != -1;
    });
    return i;
  }
};
} // namespace detail

/// An actor, whose only ability is to wait for data in associated channels. To
/// run an actor in another thread, see ActorThread
class Actor {
public:
  std::shared_ptr<detail::ActorImpl> impl;

  Actor() : impl(std::make_shared<detail::ActorImpl>()) {}

  /// Wait for data to arrive in one of n channels; returns the index of the
  /// first channel that has available data. All channels must be associated
  /// with this actor.
  template <typename... T> int wait(Channel<T> &...c) {
    return impl->wait(c...);
  }

  /// Wait for data to arrive in one of n channels with a timeout; returns the
  /// index of the first channel that has available data, or -1 if timeout_time
  /// is reached. All channels must be associated with this actor.
  template <class Clock, class Duration, typename... T>
  int wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time,
                 Channel<T> &...c) {
    return impl->wait_until(timeout_time, c...);
  }

  /// Wait for data to arrive in one of n channels with a timeout; returns the
  /// index of the first channel that has available data, or -1 if rel_time has
  /// elapsed. All channels must be associated with this actor.
  template <class Rep, class Period, typename... T>
  int wait_for(const std::chrono::duration<Rep, Period> &rel_time,
               Channel<T> &...c) {
    return impl->wait_for(rel_time, c...);
  }
};

namespace detail {

struct ChannelStoreBase {
  std::shared_ptr<detail::ActorImpl> actor_impl;
  size_t size_ = 0;

  size_t size() const { return size_; }
};

/// ChannelStore (and ChannelStoreBase) are the underlying storage which is
/// shared between Channel objects representing the same channel. It contains a
/// pointer to the owning actor (for locking/notification) and a queue of
/// elements.
///
/// The queue is implemented manually in ChannelStore (as a simple circular
/// buffer), so that the size of the queue can be accessed from
/// ChannelStoreBase (and therefore from ChannelBase) without any RTTI or
/// duplication, which isn't possible with std::queue
template <typename T> struct ChannelStore : public ChannelStoreBase {
  size_t start_ = 0;
  size_t capacity_ = 0;
  T *buf_ = nullptr;

  ChannelStore(std::shared_ptr<ActorImpl> actor_impl) {
    this->actor_impl = std::move(actor_impl);
  }

  ~ChannelStore() {
    clear();
    if (buf_ != NULL) {
      std::allocator<T>{}.deallocate(buf_, capacity_);
    }
  }

  void clear() {
    for (size_t i = 0; i < size_; i++)
      buf_[wrap(start_ + i)].~T();

    size_ = 0;
    start_ = 0;
  }

  void push(const T &value) {
    new (space_for_push()) T(value);
    size_++;
  }

  void push(T &&value) {
    new (space_for_push()) T(std::move(value));
    size_++;
  }

  template <class... Args> void emplace(Args &&...args) {
    new (space_for_push()) T(std::forward<Args>(args)...);
    size_++;
  }

  void pop() {
    assert(size_ > 0);

    buf_[start_].~T();

    start_ = wrap(start_ + 1);
    size_--;
  }

  T &front() {
    assert(size_);
    return buf_[start_];
  }

  const T &front() const {
    assert(size_);
    return buf_[start_];
  }

private:
  void resize(size_t new_capacity);

  size_t wrap(size_t i) const {
    if (i >= capacity_)
      i -= capacity_;
    return i;
  }

  /// return a pointer to the next uninitialised space
  T *space_for_push() {
    if (size_ >= capacity_)
      resize(std::max(capacity_ << 1, (size_t)1));
    size_t old_size = size_;
    return buf_ + wrap(start_ + old_size);
  }
};

/// objects which call a cleanup function when destructed, unless complete()
/// was called
///
/// this is useful to clean up after an exception occurs, without actually
/// using exception syntax (so just does nothing if exceptions are disabled)
///
/// see std::experimental::scope_fail, or std::__exception_guard in libcxx
template <typename Cleanup> struct ExceptionGuard {
  Cleanup cleanup;
  bool is_complete = false;

  ExceptionGuard(Cleanup cleanup) : cleanup(std::move(cleanup)) {}

  void complete() { is_complete = true; }

  ~ExceptionGuard() {
    if (!is_complete)
      cleanup();
  }
};

template <typename Cleanup>
ExceptionGuard<Cleanup> make_exception_guard(Cleanup cleanup) {
  return ExceptionGuard<Cleanup>(std::move(cleanup));
}

template <typename T> void ChannelStore<T>::resize(size_t new_capacity) {
  assert(new_capacity > size_);

  T *new_buf = std::allocator<T>{}.allocate(new_capacity);

  // use either memcpy, a nothrow move constructor, or the copy
  // constructor, depending on the type.

  if constexpr (std::is_trivially_copyable_v<T>) {
    if (start_ + size_ > capacity_) {
      size_t start_to_end = capacity_ - start_;
      std::memcpy(new_buf, buf_ + start_, sizeof(T) * start_to_end);
      std::memcpy(new_buf + start_to_end, buf_,
                  sizeof(T) * (size_ - start_to_end));
    } else {
      std::memcpy(new_buf, buf_ + start_, sizeof(T) * size_);
    }
  } else if constexpr (std::is_nothrow_move_constructible_v<T>) {
    for (size_t i = 0; i < size_; i++) {
      size_t old_i = wrap(start_ + i);
      new (new_buf + i) T(std::move(buf_[old_i]));
      buf_[old_i].~T();
    }
  } else {
    // if move might throw, we have to copy, and potentially handle the copy
    // constructor throwing

    // if an exception if thrown while copying, destruct the copied items and
    // free the new buffer. the exception is not caught, so the current state
    // is kept (old items not destructed, buffers not swapped)
    size_t constructed_up_to = 0;
    auto guard = detail::make_exception_guard([&]() {
      for (size_t i = 0; i < constructed_up_to; i++)
        new_buf[i].~T();
      std::allocator<T>{}.deallocate(new_buf, new_capacity);
    });

    // copy items
    for (size_t i = 0; i < size_; i++) {
      size_t old_i = wrap(start_ + i);
      new (new_buf + i) T(buf_[old_i]);
      constructed_up_to = i;
    }
    guard.complete();

    // destruct old items
    for (size_t i = 0; i < size_; i++) {
      size_t old_i = wrap(start_ + i);
      buf_[old_i].~T();
    }
  }

  if (buf_ != nullptr)
    std::allocator<T>{}.deallocate(buf_, capacity_);

  start_ = 0;
  capacity_ = new_capacity;
  buf_ = new_buf;
}

class ChannelBase {
protected:
  std::shared_ptr<detail::ChannelStoreBase> store;

public:
  ChannelBase(std::shared_ptr<ChannelStoreBase> store)
      : store(std::move(store)) {}

  /// is this non-empty? requires the associated lock to be held
  bool readable_with_lock() { return store->size() > 0; }

  /// is this non-empty?
  bool readable() {
    std::unique_lock<std::mutex> lock(store->actor_impl->mut);
    return readable_with_lock();
  }

  /// how many elements are in the channel?
  size_t size() {
    std::unique_lock<std::mutex> lock(store->actor_impl->mut);
    return store->size();
  }

  ActorImpl &actor_impl() { return *(store->actor_impl); }

protected:
  void wait_until_readable(std::unique_lock<std::mutex> &lock) {
    actor_impl().cv.wait(lock, [&] { return readable_with_lock(); });
  }
};

} // namespace detail

/// A typed channel with an unbounded number of entries
template <typename T> class Channel : public detail::ChannelBase {
public:
  using type = T;
  /// associated with a specified actor, which allows that actor to wait for
  /// this channel at the same time as others.
  Channel(Actor &actor)
      : ChannelBase(std::make_shared<detail::ChannelStore<T>>(actor.impl)) {}
  /// not associated with any actor
  Channel()
      : ChannelBase(std::make_shared<detail::ChannelStore<T>>(
            std::make_shared<detail::ActorImpl>())) {}

  void push(const T &item) {
    std::unique_lock<std::mutex> lock(actor_impl().mut);
    store_t().push(item);
    actor_impl().cv.notify_one();
  }

  void push(T &&item) {
    std::unique_lock<std::mutex> lock(actor_impl().mut);
    store_t().push(std::move(item));
    actor_impl().cv.notify_one();
  }

  template <class... Args> void emplace(Args &&...args) {
    std::unique_lock<std::mutex> lock(actor_impl().mut);
    store_t().emplace(std::forward<Args>(args)...);
    actor_impl().cv.notify_one();
  }

  /// pop an element, will throw if empty
  T pop() {
    std::unique_lock<std::mutex> lock(actor_impl().mut);
    if (!readable_with_lock())
      throw std::logic_error("called pop() on unreadable channel");
    T element = std::move(store_t().front());
    store_t().pop();
    return element;
  }

  /// pop an element, blocking if empty
  T read() {
    std::unique_lock<std::mutex> lock(actor_impl().mut);
    wait_until_readable(lock);
    T element = std::move(store_t().front());
    store_t().pop();
    return element;
  }

  /// remove all elements
  void clear() {
    std::unique_lock<std::mutex> lock(actor_impl().mut);
    store_t().clear();
  }

private:
  detail::ChannelStore<T> &store_t() {
    return static_cast<detail::ChannelStore<T> &>(*store);
  }
};

namespace detail {
/// just used for checking that ActorThread isn't applied more than once
class IActorThread {};
} // namespace detail

/// Wrapper around a class derived from Actor, which runs its `void run()`
/// method in a thread, and its `void exit()` method in the destructor. For the
/// thread to be cleaned up, `exit` must cause `run` to return.
///
/// This can't be implemented nicely through regular inheritance, because the
/// constructor of a base class can't safely call derived methods, so we can't
/// start the thread from the constructor.
template <typename ActorT>
class ActorThread : public ActorT, private detail::IActorThread {
public:
  template <typename... Args>
  ActorThread(Args &&...args)
      : ActorT(std::forward<Args>(args)...), thread([&] { this->run(); }) {}

  ~ActorThread() {
    this->exit();
    thread.join();
  }

private:
  std::thread thread;

  static_assert(!std::is_base_of<detail::IActorThread, ActorT>::value,
                "ActorThread must only be applied once to an Actor");
};

} // namespace actorpp
