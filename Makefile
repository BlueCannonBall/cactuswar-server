CC=g++
CFLAGS=-luv -lssl -lcrypto -pthread -std=c++14 -Wall -Wno-unknown-pragmas -s -Ofast
TARGET=./build/server
OBJDIR=build/obj

server: build/main.o build/ws28/%.o
	mkdir -p build
	$(CC) $(OBJDIR)/*.o $(OBJDIR)/ws28/*.o $(CFLAGS) -o $(TARGET)

build/main.o: core.hpp quadtree.hpp streampeerbuffer.hpp bcblog.hpp entityconfig.hpp
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	$(CC) -c main.cpp $(CFLAGS) -o $(OBJDIR)/main.o

build/ws28/%.o:
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	@mkdir -p build/obj/ws28
	cd $(OBJDIR)/ws28 && $(CC) -c ../../../ws28/*.cpp $(CFLAGS)

run: server
	$(TARGET) 8000

clean:
	$(RM) -r build
