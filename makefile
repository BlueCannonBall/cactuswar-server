all:
		mkdir -p build
		g++ -o build/kakto main.cpp ws28/*.cpp -luv -lssl -lcrypto -pthread -std=c++14 -Wall -Wno-unknown-pragmas -s -O2
run: all
		./build/kakto 8000
