CC=g++
LDFLAGS=-luv -lssl -lcrypto -pthread -ltcmalloc
CFLAGS=-std=c++14 -Wall -Wno-unknown-pragmas -g -Ofast -march=native \
	-mtune=native -fno-signed-zeros -fno-trapping-math -finline-functions -ftree-vectorize \
	-frename-registers -funroll-loops -fno-builtin-malloc -fno-builtin-calloc  \
	-fno-builtin-realloc -fno-builtin-free -fopenmp-simd -Bsymbolic -fno-semantic-interposition
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
	rm -rf build
	rm -rf **.gch
