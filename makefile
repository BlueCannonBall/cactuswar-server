all:
		ccache g++ -o dbserver main.cpp ws28/*.cpp -luv -lssl -lcrypto -std=c++14 -Wall -s -Ofast
