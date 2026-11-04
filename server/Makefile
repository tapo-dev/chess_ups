CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

# Every .cpp in this directory is a module of the server
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: server

server: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

-include $(OBJS:.o=.d)

clean:
	rm -f server *.o *.d
