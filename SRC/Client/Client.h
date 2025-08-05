#ifndef SIMPLECHAT_CLIENT_H
#define SIMPLECHAT_CLIENT_H
#include <VORTEX_MP/LinearAlg>

#include "../Util/AsioInclude.h"


class SimpleClient {
public:
   SimpleClient();
   
   void connect(const std::string& ip, ushort_16 port);
   void send(const std::string& msg);
   void startThread();
private:
   asio::error_code ec;
   asio::io_context context;
   tcp::resolver resolver;
   tcp::socket socket;
   ushort_16 port_ = 0;
   std::string host_;

   std::vector<ubyte_8> readBuffer;
   std::vector<ubyte_8> writeBuffer;

   void readData();
public:
   std::thread thrContext;
   static void printClient(std::string&& clientStr,const std::string& ip = "localhost",ushort_16 port = 0, bool wPort = false);
};

#endif // SIMPLECHAT_CLIENT_H
