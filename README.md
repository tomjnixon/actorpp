actorpp
=======

An ad-hoc, informally-specified, bug-ridden, slow implementation of half of
Erlang in C++11.

This library is intended to allow simple message-passing communication between
threads, minimising dependencies and trickery, while being complete
enough to be useful.

development
-----------

Build and test with:

```
cmake -G Ninja -B build . -DCMAKE_BUILD_TYPE=Debug
ninja -C build && ninja -C build test
```

For now, you will need to run `python test/test_server.py` while running the
net tests; this should be replaced with a C++ implementation.

license
-------

```
Copyright 2023 Thomas Nixon

This program is free software: you can redistribute it and/or modify it under the terms of version 3 of the GNU General Public License as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

See LICENSE.
```
