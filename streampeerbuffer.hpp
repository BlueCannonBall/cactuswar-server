#include <vector>
#include <array>
#include <string>

#pragma once

namespace spb
{
    class StreamPeerBuffer
    {
        private:
          template <typename T>
          std::array<unsigned char, sizeof(T)> to_bytes(const T &object);

          template <typename T>
          T &from_bytes(const std::array<unsigned char, sizeof(T)> &bytes,
                        T &object);

          template <typename T>
          T bswap(T val);
        
        public:
          std::vector<unsigned char> data_array;
          unsigned int offset = 0;
          bool endian_swap = false;
          StreamPeerBuffer(bool);

          void put_u8(unsigned char);
          void put_u16(unsigned short int);
          void put_u32(unsigned int);

          void put_8(char);
          void put_16(short int);
          void put_32(int);

          unsigned char get_u8();
          unsigned short int get_u16();
          unsigned int get_u32();

          char get_8();
          short int get_16();
          int get_32();

          void put_string(std::string&);
          std::string get_string();

          void put_float(float);
          float get_float();
    };
}
