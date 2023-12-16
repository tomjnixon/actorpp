#!/bin/bash
find include/ test/ -name '*.cpp' -or -name '*.hpp' | xargs clang-format -i
