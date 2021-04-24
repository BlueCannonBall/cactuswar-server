CC=g++
CFLAGS=-luv -lssl -lcrypto -pthread -std=c++14 -Wall -Wno-unknown-pragmas -s -O2

server: main.cpp ws28/*.cpp core.hpp quadtree.hpp streampeerbuffer.hpp bcblog.hpp
	mkdir -p build
	$(CC) -o build/server main.cpp ws28/*.cpp $(CFLAGS)

run: server
	./build/server 8000

clean:
	$(RM) build/server
