CC = g++
LDLIBS = -luv -lssl -lcrypto -lleveldb -lfazo
CFLAGS = -std=c++14 -Wall -Llib -s -Ofast -march=native \
	-fno-signed-zeros -fno-trapping-math -finline-functions \
	-frename-registers -funroll-loops -fmerge-all-constants \
	-fno-semantic-interposition -fdiagnostics-color=always \
	-ftree-vectorize -fno-stack-protector -fif-conversion  \
	-finline-functions-called-once -finline-small-functions \
	-Bsymbolic -fif-conversion2 -mtune=native
TARGET = ./build/server
OBJDIR = build/obj
PORT = 8000

ifdef THREADING
CFLAGS += -DTHREADING=$(THREADING) -pthread
endif
ifdef DEBUG_MAINLOOP_SPEED
CFLAGS += -DDEBUG_MAINLOOP_SPEED=$(DEBUG_MAINLOOP_SPEED)
endif

$(TARGET): $(OBJDIR)/main.o $(OBJDIR)/ws28/*.o $(OBJDIR)/streampeerbuffer.o json.hpp.gch fazo.h.gch logger.hpp.gch threadpool.hpp.gch
	mkdir -p build
	$(CC) $(OBJDIR)/*.o $(OBJDIR)/ws28/*.o $(LDLIBS) $(CFLAGS) -o $@

$(OBJDIR)/main.o: main.cpp core.hpp entityconfig.hpp fazo.h bcblog.hpp json.hpp streampeerbuffer.hpp logger.hpp threadpool.hpp
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
	$(CC) $< $(CFLAGS)

fazo.h.gch: fazo.h
	$(CC) $< $(CFLAGS)

logger.hpp.gch: logger.hpp
	$(CC) $< $(CFLAGS)

threadpool.hpp.gch: threadpool.hpp
	$(CC) $< $(CFLAGS)

.PHONY: run clean

run: $(TARGET)
	$(TARGET) $(PORT)

clean:
	rm -rf $(TARGET)
	rm -rf build
	rm -rf **.gch **.pch
	rm -rf **.o
