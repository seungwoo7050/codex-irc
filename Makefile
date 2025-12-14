CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS =

SRC = src/main.cpp src/server.cpp src/protocol/framer.cpp

all: modern-irc

modern-irc: $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

clean:
	rm -f modern-irc tests/unit/framer_test

.PHONY: all clean test e2e

test: modern-irc tests/unit/framer_test
	./tests/unit/framer_test

# Unit test binary

tests/unit/framer_test: tests/unit/framer_test.cpp src/protocol/framer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

e2e: modern-irc
	python3 tests/e2e/smoke_test.py
