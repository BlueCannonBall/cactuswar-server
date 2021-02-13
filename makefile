all:
		ccache g++ -o dfserver main.cpp ws28/*.cpp -luv -lssl -lcrypto -std=c++14 -s