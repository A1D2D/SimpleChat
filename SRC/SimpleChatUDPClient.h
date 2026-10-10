#ifndef SIMPLECHAT_UDP_CLIENT_H
#define SIMPLECHAT_UDP_CLIENT_H

#include <StreamedNet.h>
#include "Instance.h"

class SCUClient : public SN::UDPClient, public Instance {
public:
   SCUClient(SN::Context context, std::shared_ptr<MessageCallback> callback) : SN::UDPClient(context), Instance(callback) {}

   void onConnect() override {
      if(messageCallback) (*messageCallback)("connected\n", MessageType::Info);
      messages.push_back(ChatMessage("connected\n", MessageType::Info));
      startRead();
   }

   void onRead(std::vector<uint8_t> msg) override {
      std::string text(msg.begin(), msg.end());
      if(messageCallback) (*messageCallback)("message received from server: " + text + "\n", MessageType::Info);
      messages.push_back(ChatMessage(text + "\n", MessageType::Server));
   }

   void send(const std::vector<uint8_t>& msg) override {
      SN::UDPClient::send(msg);
      std::string text(msg.begin(), msg.end());
      messages.push_back(ChatMessage(text + "\n", MessageType::Client));
      if(messageCallback) (*messageCallback)("sending:" + text + "\n", MessageType::Info);
   }

   void disconnect() override {
      SN::UDPClient::disconnect();
   }

   void onDisconnect() override {
      if(messageCallback) (*messageCallback)("client disconnected from server\n", MessageType::Info);
      messages.push_back(ChatMessage("client disconnected from server\n", MessageType::Info));
   }
};

#endif // ~SIMPLECHAT_UDP_CLIENT_H