actorpp -- simple concurrency in C++
====================================

.. namespace:: actorpp

This is the documentation for `actorpp`, C++ library for implementing
concurrent systems with message passing.

`actorpp` implements a go-like model, in which threads communicate by sharing
:class:`Channel` objects. Many threads can push data to a channel, while only
one reads from it.

To wait for data on multiple channels, all channels need to share the same
mutex and condition variable. These are stored in an :class:`Actor` instance,
to which the channels that a thread wants to wait on are associated.

Typically, the way this is used is to subclass :class:`Actor` to implement the
required functionality for a single thread, and wrap this subclass in
:class:`ActorThread` to actually run them.

Subclasses of :class:`Actor` should accept the channels that they will write to
as constructor arguments (since they will be associated with other actors), and
should create and expose the channels which they will receive from (for the
opposite reason).

For example, this class receives integers on one channel (ping), and sends them
on another (pong):

.. literalinclude:: ../../test/basic_tests.cpp
   :start-after: // example_start
   :end-before: // example_end

Note how:

- pong is passed into the constructor, so it can be associated with another
  actor
- `ping` and `do_exit` are associated with `this`, and are externally
  accessible
- `run` waits on both `ping` and `do_exit`, which may be pushed to from
  different threads

For simplicity and portability, `actorpp` doesn't implement any fancy
concurrency model -- it just uses plain threads, so is not suitable for any
uses that can't cope with the overhead of one thread per actor.

It really exists to formalise the kind of concurrency which works well and is
scalable (in terms of mental/testing overhead) for non-high-performance
systems.

This was built as part of the :ref:`ESHET <eshet:eshet_home>` home automation
system; it's what all concurrency in :ref:`eshetcpp <eshetcpp:eshetcpp_home>`
is implemented with.

The core of `actorpp` only uses standard C++11 constructs, so should be very
portable, while the networking library just uses UNIX sockets. It is regularly
used on x86 linux, and ESP32 with freertos.

`actorpp` is hosted on github at https://github.com/tomjnixon/actorpp\.

.. toctree::
   :maxdepth: 3
   :caption: Contents:

   readmes
   api/library_root

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
