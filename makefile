all:
		mkdir -p build
		g++ -o build/dbserver main.cpp ws28/*.cpp -luv -lssl -lcrypto -pthread -std=c++14 -Wall -Wno-unknown-pragmas -s -O2
run: all
		./build/dbserver 8000
