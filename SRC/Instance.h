#ifndef SIMPLECHAT_INSTANCE_H
#define SIMPLECHAT_INSTANCE_H

#include <cstdint>
#include <iostream>
#include <vector>

class Instance {
public:
   virtual ~Instance() = default;
   virtual void send(const std::vector<uint8_t>& msg) {
      std::cout << "undefined object sent failed\n";
   }
   virtual void disconnect() {
      std::cout << "undefined object cant be disconnected\n";
   }
};

#endif // ~INSTANCE_H