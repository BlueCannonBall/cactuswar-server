all:
		mkdir -p build
		g++ -o build/dbserver main.cpp ws28/*.cpp -luv -lssl -lcrypto -lpthread -std=c++14 -Wall -Wno-unknown-pragmas -s -O2 -flto
run: all
		./build/dbserver 8000
