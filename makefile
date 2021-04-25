CC=g++
CFLAGS=-luv -lssl -lcrypto -pthread -std=c++14 -Wall -Wno-unknown-pragmas -s -O2
TARGET=./build/server

server: main.cpp ws28/*.cpp core.hpp quadtree.hpp streampeerbuffer.hpp bcblog.hpp entityconfig.hpp
	mkdir -p build
	$(CC) -o $(TARGET) main.cpp ws28/*.cpp $(CFLAGS)

run: server
	$(TARGET) 8000

clean:
	$(RM) $(TARGET)
