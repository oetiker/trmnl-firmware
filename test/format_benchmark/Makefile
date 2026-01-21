# Format Benchmark Makefile

CXX = g++
CXXFLAGS = -O2 -Wall -std=c++11

all: benchmark

benchmark: benchmark.cpp planar_g5.h rle_gray.h
	$(CXX) $(CXXFLAGS) -o benchmark benchmark.cpp

run: benchmark
	./benchmark

clean:
	rm -f benchmark

.PHONY: all run clean
