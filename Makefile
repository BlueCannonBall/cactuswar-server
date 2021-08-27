CC=c++
LDFLAGS=-luv -lssl -lcrypto -pthread -lleveldb
CFLAGS=-std=c++14 -Wall -Wno-unknown-pragmas -s -Ofast -march=native \
	-fno-signed-zeros -fno-trapping-math -fdiagnostics-color=always  \
	-frename-registers -funroll-loops -fmerge-all-constants -ftree-vectorize  \
	-fopenmp-simd -Bsymbolic -fno-semantic-interposition -mtune=native
TARGET=./build/server
OBJDIR=build/obj
PORT=8000

$(TARGET): $(OBJDIR)/main.o $(OBJDIR)/ws28/*.o $(OBJDIR)/streampeerbuffer.o json.hpp.gch quadtree.hpp.gch
	mkdir -p build
	$(CC) $(OBJDIR)/*.o $(OBJDIR)/ws28/*.o $(LDFLAGS) $(CFLAGS) -o $@

$(OBJDIR)/main.o: main.cpp core.hpp entityconfig.hpp quadtree.hpp bcblog.hpp json.hpp streampeerbuffer.hpp
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	$(CC) -c main.cpp $(CFLAGS) -o $@

$(OBJDIR)/streampeerbuffer.o: streampeerbuffer.cpp streampeerbuffer.hpp
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	$(CC) -c streampeerbuffer.cpp $(CFLAGS) -o $@

$(OBJDIR)/ws28/*.o: ws28/src/*
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/ws28
	cd $(OBJDIR)/ws28 && $(CC) -c ../../../ws28/src/*.cpp $(CFLAGS)

json.hpp.gch: json.hpp
	$(CC) json.hpp $(CFLAGS)

quadtree.hpp.gch: quadtree.hpp
	$(CC) quadtree.hpp $(CFLAGS)

.PHONY: run clean

run: $(TARGET)
	$(TARGET) $(PORT)

clean:
	rm -rf $(TARGET)
	rm -rf build
	rm -rf **.gch **.pch
	rm -rf **.o
