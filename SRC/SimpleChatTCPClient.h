#ifndef SIMPLECHAT_TCP_CLIENT_H
#define SIMPLECHAT_TCP_CLIENT_H

#include <StreamedNet.h>
#include "Instance.h"

class SCTClient : public SN::TCPClient, public Instance {
public:
   SCTClient(SN::Context context) : SN::TCPClient(context) {}

   void onConnect() override {
      std::cout << "connected\n";
      startRead();
   }

   void onRead(std::vector<uint8_t> msg) override {
      std::string text(msg.begin(), msg.end());
      std::cout << "message received from server: " << text << "\n";
   }

   void send(const std::vector<uint8_t>& msg) override {
      SN::TCPClient::send(msg);
   }

   void disconnect() override {
      SN::TCPClient::disconnect();
   }
};

#endif // ~SIMPLECHAT_TCP_CLIENT_H