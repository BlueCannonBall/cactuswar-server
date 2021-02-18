all:
		mkdir -p build
		g++ -o build/dbserver main.cpp ws28/*.cpp -luv -lssl -lcrypto -std=c++14 -Wall -s -O1
