#ifndef SIMPLECHAT_CONNECTION_H
#define SIMPLECHAT_CONNECTION_H
#include <VORTEX_MP/LinearAlg>

#include "../Util/AsioInclude.h"

class SimpleConnection {
public:
   SimpleConnection(tcp::socket& accepted);

   std::function<void(const std::string&)> onMessage;

   tcp::socket socket;

   void readData();
   void send(const std::string& msg);

private:
   std::vector<ubyte_8> readBuffer;
   std::vector<ubyte_8> writeBuffer;
};

#endif //SIMPLECHAT_CONNECTION_H