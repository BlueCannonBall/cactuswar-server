CC=g++
LINKS=-luv -lssl -lcrypto -pthread -ltcmalloc_minimal
CFLAGS=-std=c++14 -Wall -Wno-unknown-pragmas -fdiagnostics-color=always \
	 -g -Ofast -march=native -mtune=native -fno-signed-zeros \
	-fno-trapping-math -frename-registers -funroll-loops -fno-builtin-malloc -fno-builtin-calloc \
	-fno-builtin-realloc -fno-builtin-free -fopenmp-simd
TARGET=./build/server
OBJDIR=build/obj

$(TARGET): $(OBJDIR)/main.o $(OBJDIR)/ws28/*.o $(OBJDIR)/streampeerbuffer.o json.hpp.gch
	mkdir -p build
	$(CC) $(OBJDIR)/*.o $(OBJDIR)/ws28/*.o $(LINKS) $(CFLAGS) -o $(TARGET)

$(OBJDIR)/main.o: main.cpp core.hpp entityconfig.hpp quadtree.hpp bcblog.hpp json.hpp
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	$(CC) -c main.cpp $(LINKS) $(CFLAGS) -o $(OBJDIR)/main.o

$(OBJDIR)/streampeerbuffer.o: streampeerbuffer.cpp streampeerbuffer.hpp
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	$(CC) -c streampeerbuffer.cpp $(LINKS) $(CFLAGS) -o $(OBJDIR)/streampeerbuffer.o

$(OBJDIR)/ws28/*.o: $(wildcard ws28/**/*)
	@mkdir -p build
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/ws28
	cd $(OBJDIR)/ws28 && $(CC) -c ../../../ws28/*.cpp $(LINKS) $(CFLAGS)

json.hpp.gch: json.hpp
	$(CC) json.hpp $(CFLAGS)

run: $(TARGET)
	$(TARGET) 8000

clean:
	rm -rf build
	rm -rf *.gch ws28/*.gch

cleanobj:
	rm -rf $(OBJDIR)/*.o
	rm -rf $(OBJDIR)/ws28/*.o
