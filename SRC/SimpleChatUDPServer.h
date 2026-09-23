#ifndef SIMPLECHAT_UDP_SERVER_H
#define SIMPLECHAT_UDP_SERVER_H

#include <StreamedNet.h>
#include "Instance.h"

class SCUConnection : public SN::UDPConnection {
public:
   SCUConnection(SN::UDPServer* server, SN::UDPServer::UdpHandle handle) : SN::UDPConnection(server, handle) {}

   void onRead(std::vector<uint8_t> msg) override {
      std::string text(msg.begin(), msg.end());
      std::cout << "message received from client" << ": " << text << "\n";
   }
};

class SCUServer : public SN::UDPServer, public Instance {
public:
   SCUServer(SN::Context context) : SN::UDPServer(context) {}

   std::shared_ptr<SN::UDPConnection> onAccept(SN::UDPServer::UdpHandle handle, std::vector<uint8_t> msg) override {
      std::string text(msg.begin(), msg.end());
      if(text == "hellNah") return nullptr;
      std::cout << text << "\n";
      std::cout << "connect msg: " << text << "\n";
      return std::make_shared<SCUConnection>(this, std::move(handle));
   }

   void onStart() override {
      std::cout << "start accept\n";
      startRead();
   }

   void send(const std::vector<uint8_t>& msg) override {
      SN::UDPServer::send(msg);
   }

   void disconnect() override {
      SN::UDPServer::disconnect();
   }
};

#endif // ~SIMPLECHAT_UDP_SERVER_H