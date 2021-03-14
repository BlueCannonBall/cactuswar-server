all:
		mkdir -p build
		g++ -o build/server main.cpp core.hpp ws28/*.cpp -luv -lssl -lcrypto -pthread -std=c++14 -Wall -Wno-unknown-pragmas -s -O2
run: all
		./build/kakto 8000
