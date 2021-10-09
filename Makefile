CC=c++
LDLIBS=-luv -lssl -lcrypto -pthread -lleveldb -lfazo
CFLAGS=-std=c++14 -Wall -s -Ofast -march=native -mtune=native \
	-fno-signed-zeros -fno-trapping-math -finline-functions \
	-frename-registers -funroll-loops -fmerge-all-constants \
	-fopenmp-simd -Bsymbolic -fno-semantic-interposition \
	-fdiagnostics-color=always -funsafe-math-optimizations \
	-ftree-vectorize -Llib
TARGET=./build/server
OBJDIR=build/obj
PORT=8000

$(TARGET): $(OBJDIR)/main.o $(OBJDIR)/ws28/*.o $(OBJDIR)/streampeerbuffer.o json.hpp.gch fazo.h.gch
	mkdir -p build
	$(CC) $(OBJDIR)/*.o $(OBJDIR)/ws28/*.o $(LDLIBS) $(CFLAGS) -o $@

$(OBJDIR)/main.o: main.cpp core.hpp entityconfig.hpp fazo.h bcblog.hpp json.hpp streampeerbuffer.hpp
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

fazo.h.gch: fazo.h
	$(CC) fazo.h $(CFLAGS)

.PHONY: run clean

run: $(TARGET)
	$(TARGET) $(PORT)

clean:
	rm -rf $(TARGET)
	rm -rf build
	rm -rf **.gch **.pch
	rm -rf **.o
