CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS =

SRC = src/main.cpp src/server.cpp src/protocol/framer.cpp src/protocol/message.cpp

all: modern-irc

modern-irc: $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

clean:
	rm -f modern-irc tests/unit/framer_test tests/unit/message_test

.PHONY: all clean test e2e

test: modern-irc tests/unit/framer_test tests/unit/message_test
	./tests/unit/framer_test
	./tests/unit/message_test

# Unit test binary

tests/unit/framer_test: tests/unit/framer_test.cpp src/protocol/framer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

tests/unit/message_test: tests/unit/message_test.cpp src/protocol/message.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

e2e: modern-irc
	python3 -m unittest discover -s tests -p "test_*.py"
