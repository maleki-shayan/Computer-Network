CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: sender receiver

sender: sender.cpp packet.hpp timer.hpp logger.hpp
	$(CXX) $(CXXFLAGS) sender.cpp -o sender

receiver: receiver.cpp packet.hpp timer.hpp logger.hpp
	$(CXX) $(CXXFLAGS) receiver.cpp -o receiver

clean:
	rm -f sender receiver sample_output.txt sample_output.bin
	rm -rf logs