all:
		mkdir -p build
		g++ -o build/cwserver main.cpp ws28/*.cpp -luv -lssl -lcrypto -pthread -std=c++14 -Wall -Wno-unknown-pragmas -s -O2
run: all
		./build/cacti 8000
