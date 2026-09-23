#ifndef SIMPLECHAT_UDP_CLIENT_H
#define SIMPLECHAT_UDP_CLIENT_H

#include <StreamedNet.h>
#include "Instance.h"

class SCUClient : public SN::UDPClient, public Instance {
public:
   SCUClient(SN::Context context) : SN::UDPClient(context) {}

   void onConnect() override {
      std::cout << "connected\n";
      startRead();
   }

   void onRead(std::vector<uint8_t> msg) override {
      std::string text(msg.begin(), msg.end());
      std::cout << "message received from server: " << text << "\n";
   }

   void send(const std::vector<uint8_t>& msg) override {
      SN::UDPClient::send(msg);
   }

   void disconnect() override {
      SN::UDPClient::disconnect();
   }
};

#endif // ~SIMPLECHAT_UDP_CLIENT_H