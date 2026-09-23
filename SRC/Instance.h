#ifndef SIMPLECHAT_INSTANCE_H
#define SIMPLECHAT_INSTANCE_H

#include <iostream>

class Instance {
public:
   virtual ~Instance() = default;
   virtual void disconnect() {
      std::cout << "undefined object cant be disconnected\n";
   }
};

#endif // ~INSTANCE_H