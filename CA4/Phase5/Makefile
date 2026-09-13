CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic

all: sender receiver

sender: sender.cpp experiment.hpp packet.hpp timer.hpp logger.hpp
	$(CXX) $(CXXFLAGS) sender.cpp -o sender

receiver: receiver.cpp experiment.hpp packet.hpp timer.hpp logger.hpp
	$(CXX) $(CXXFLAGS) receiver.cpp -o receiver

sample:
	python3 prepare_input.py

plots:
	python3 plot_results.py

test: all
	bash run_test.sh

clean:
	rm -f sender receiver sample_output.bin

reset: clean
	rm -rf results
