CC=g++
CFLAGS=-luv -lssl -lcrypto -pthread -std=c++14 -Wall -Wno-unknown-pragmas -s -Ofast -march=native -mtune=native -fno-signed-zeros -fno-trapping-math -frename-registers -funroll-loops
TARGET=./build/server
OBJDIR=build/obj

$(TARGET): $(OBJDIR)/main.o $(OBJDIR)/ws28/*.o $(OBJDIR)/streampeerbuffer.o
	mkdir -p build
	$(CC) $(OBJDIR)/*.o $(OBJDIR)/ws28/*.o $(CFLAGS) -o $(TARGET)

$(OBJDIR)/main.o: main.cpp core.hpp entityconfig.hpp quadtree.hpp bcblog.hpp
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	$(CC) -c main.cpp $(CFLAGS) -o $(OBJDIR)/main.o

$(OBJDIR)/streampeerbuffer.o: streampeerbuffer.cpp streampeerbuffer.hpp
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	$(CC) -c streampeerbuffer.cpp $(CFLAGS) -o $(OBJDIR)/streampeerbuffer.o

$(OBJDIR)/ws28/*.o: $(wildcard ws28/**/*)
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/ws28
	cd $(OBJDIR)/ws28 && $(CC) -c ../../../ws28/*.cpp $(CFLAGS)

run: $(TARGET)
	$(TARGET) 8000

clean:
	rm -r build

cleanobj:
	rm $(OBJDIR)/*.o
	rm $(OBJDIR)/ws28/*.o
