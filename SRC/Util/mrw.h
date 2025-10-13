#ifndef MRW_MEMORY_READ_WRITE_H
#define MRW_MEMORY_READ_WRITE_H

#include <iostream>
#include <vector>
#include <array>

namespace MRW {
   template<typename T>
   T readMemory(std::vector<unsigned char>& memory, int offsetInBytes = 0) {
      T t;
      int size = sizeof(T);
      if (offsetInBytes + size > memory.size()) {
         std::cerr << "read outOfBounds" << std::endl;
         return {};
      }
      memcpy(&t, memory.data() + offsetInBytes, size);
      return t;
   }

   template<typename T>
   std::vector<T> readMemoryArray(std::vector<unsigned char>& memory, int count, int offsetInBytes = 0) {
      int size = sizeof(T) * count;
      if (offsetInBytes + size > memory.size()) {
         std::cerr << "Array: read outOfBounds" << std::endl;
         return {};
      }
      std::vector<T> t(count);
      memcpy(t.data(), memory.data() + offsetInBytes, size);
      return t;
   }

   template<typename T>
   void writeMemory(std::vector<unsigned char>& memory, T value, int offsetInBytes = 0) {
      int size = sizeof(T);
      if (offsetInBytes + size > memory.size()) {
         memory.resize(offsetInBytes + size);
      }
      memcpy(memory.data() + offsetInBytes, &value, size);
   }

   template<typename T>
   void writeMemory(std::vector<unsigned char>& memory, const std::vector<T>& values, int offsetInBytes = 0) {
      int size = sizeof(T) * values.size();
      if (offsetInBytes + size > memory.size()) {
         memory.resize(offsetInBytes + size);
      }
      memcpy(memory.data() + offsetInBytes, values.data(), size);
   }

   inline std::array<unsigned char,2> getHex(unsigned char byte) {
      const unsigned char HexChars[6] = {'A','B','C','D','E','F'};
      std::array<unsigned char, 2> array{};
      unsigned char bits = byte & 0x0F;
      for (int i = 0; i < 2; ++i) {
         if(i == 0) {
            bits = (byte & 0xF0) >> 4;
         }
         if(bits < 10) {
            array[i] = '0' + bits;
         } else array[i] = HexChars[bits-10];
      }
      return array;
   }

   inline std::string getHex(const std::vector<unsigned char>& buffer) {
      std::string hexString(buffer.size()*3-1,' ');
      for (int i = 0; i < buffer.size(); ++i) {
         auto hexChars = getHex(buffer[i]);
         hexString[i*3] = (char)hexChars[0];
         hexString[i*3+1] = (char)hexChars[1];
      }
      return hexString;
   }
}

#endif //MRW_MEMORY_READ_WRITE_H
