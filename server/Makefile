CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

.PHONY: all clean

all: server

server: server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f server
