CXX=g++
CXXFLAGS=-Ivendor/include -Wall -std=c++11 -g -O1
LDFLAGS=

OBJS=simpeg2.o vendor/src/format.o

all: simpeg2

%.o: %.cc
	$(CXX) -o $@ -c $< $(CXXFLAGS)

simpeg2: $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^

.PHONY: clean
clean:
	rm -f simpeg2 $(OBJS)

.PHONY: test
test: simpeg2
	./simpeg2 denki.nut
