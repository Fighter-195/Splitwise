# Splitwise (C++) — build the command-line app.
#
#   make          build ./splitwise (or splitwise.exe on Windows)
#   make run      build and run
#   make clean    remove build artifacts

CXX      ?= g++
CXXFLAGS ?= -std=c++14 -Wall -Wextra -O2

SRCS := $(wildcard engine/*.cpp) $(wildcard storage/*.cpp) cli/main.cpp
BIN  := splitwise

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRCS)

.PHONY: run clean
run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) $(BIN).exe *.o
