CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

all: run_simple_ai

run_simple_ai: tetris_core.cpp simple_ai.cpp run_simple_ai.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f run_simple_ai

.PHONY: all clean
