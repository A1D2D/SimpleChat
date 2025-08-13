#ifndef SIMPLECHAT_CLIENT_H
#define SIMPLECHAT_CLIENT_H
#include <VORTEX_MP/LinearAlg>

#include "../Util/AsioInclude.h"


class SimpleClient {
public:
   enum State {
      Offline = 0,
      Online = FlagDef(1),
      Connecting = FlagDef(2)
   };

public:
   SimpleClient();
   
   void connect(const std::string& ip, ushort_16 port);
   void send(const std::vector<ubyte_8>& msg);
   void send(const std::string& msg);
   void disconnect();
   void joinThread();
   
   asio::io_context& getContext();
   ushort_16 getPort();
   const std::string getIp();
   // const tcp::endpoint& getEndpoints();

protected:
   virtual void onConnect() {}
   virtual void onDisconnect() {}
   virtual void onReceive(const std::vector<ubyte_8>& data) {}

private:
   asio::error_code ec;
   asio::io_context context;
   tcp::resolver resolver;
   tcp::socket socket;
   ushort_16 port_ = 0;
   std::string host_;
   // tcp::endpoint endpoints;

   VArray<ubyte_8> readBuffer;
   VArray<ubyte_8> writeBuffer;

   void readData();
   void clientAbort();
   void startThread();
public:
   std::thread thrContext;
   std::atomic<ubyte_8> state = State::Offline;
   static void printClient(std::string&& clientStr,const std::string& ip = "localhost",ushort_16 port = 0, bool wPort = false);
};

#endif // SIMPLECHAT_CLIENT_H
