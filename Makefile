CC=g++
CFLAGS=-luv -lssl -lcrypto -pthread -std=c++14 -Wall -Wno-unknown-pragmas -s -Ofast
TARGET=./build/server

server: build/main.o build/ws28/%.o
	mkdir -p build
	$(CC) build/*.o build/ws28/*.o $(CFLAGS) -o build/server

build/main.o: core.hpp quadtree.hpp streampeerbuffer.hpp bcblog.hpp entityconfig.hpp
	mkdir -p build
	$(CC) -c main.cpp $(CFLAGS) -o build/main.o

build/ws28/%.o:
	mkdir -p build
	mkdir -p build/ws28
	cd build/ws28 && $(CC) -c ../../ws28/*.cpp $(CFLAGS)

run: server
	$(TARGET) 8000

clean:
	$(RM) $(TARGET)
