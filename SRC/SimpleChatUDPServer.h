#ifndef SIMPLECHAT_UDP_SERVER_H
#define SIMPLECHAT_UDP_SERVER_H

#include <StreamedNet.h>
#include "Instance.h"
#include <vector>

class SCUConnection : public SN::UDPConnection {
public:
   SCUConnection(SN::UDPServer* server, SN::UDPServer::UdpHandle handle, std::shared_ptr<MessageCallback> callback) : SN::UDPConnection(server, handle), callback(callback) {}

   void onRead(std::vector<uint8_t> msg) override;

   void onDisconnect() override;
   
   void sendAsConnection(const std::vector<uint8_t>& msg);

   std::shared_ptr<MessageCallback> callback;
   int id;
};

class SCUServer : public SN::UDPServer, public Instance {
public:
   SCUServer(SN::Context context, std::shared_ptr<MessageCallback> callback) : SN::UDPServer(context), Instance(callback) {}

   void onStart() override {
      if(messageCallback) (*messageCallback)("start accept\n", MessageType::Info);
      messages.push_back(ChatMessage("start accept\n", MessageType::Info));
      startRead();
   }

   std::shared_ptr<SN::UDPConnection> onAccept(SN::UDPServer::UdpHandle handle, std::vector<uint8_t> msg) override {
      std::error_code ec;
      
      std::string text(msg.begin(), msg.end());
      if(messageCallback) (*messageCallback)(text + "\n", MessageType::Info);
      if(messageCallback) (*messageCallback)("connect msg: " + text + "\n", MessageType::Info);

      if(messageCallback) (*messageCallback)("=== Socket ===\n", MessageType::Info);
      if(messageCallback) (*messageCallback)(std::string("Open: ") + (handle.socket->is_open() ? "true" : "false") + "\n", MessageType::Info);
      if(messageCallback) (*messageCallback)("Native handle: " + std::to_string(handle.socket->native_handle()) + "\n", MessageType::Info);

      messages.push_back(ChatMessage(text + "\n", MessageType::Info));
      messages.push_back(ChatMessage("connect msg: " + text + "\n", MessageType::Info));

      messages.push_back(ChatMessage("=== Socket ===\n", MessageType::Info));
      messages.push_back(ChatMessage(std::string("Open: ") + (handle.socket->is_open() ? "true" : "false") + "\n", MessageType::Info));
      messages.push_back(ChatMessage("Native handle: " + std::to_string(handle.socket->native_handle()) + "\n", MessageType::Info));
      
      auto local = handle.socket->local_endpoint(ec);
      if (!ec && messageCallback) (*messageCallback)("Local: " + local.address().to_string() + ":" + std::to_string(local.port()) + "\n", MessageType::Info);
      if (!ec) messages.push_back(ChatMessage("Local: " + local.address().to_string() + ":" + std::to_string(local.port()) + "\n", MessageType::Info));
      ec.clear();

      auto remote = handle.socket->remote_endpoint(ec);
      if (!ec && messageCallback) (*messageCallback)("Remote: " + remote.address().to_string() + ":" + std::to_string(remote.port()) + "\n", MessageType::Info);
      if (!ec) messages.push_back(ChatMessage("Remote: " + remote.address().to_string() + ":" + std::to_string(remote.port()) + "\n", MessageType::Info));
      
      if(messageCallback) (*messageCallback)(std::string(handle.socket->local_endpoint().protocol() == udp::v4() ? "UDP/IPv4" : "UDP/IPv6") + "\n", MessageType::Info);
      messages.push_back(ChatMessage(std::string(handle.socket->local_endpoint().protocol() == udp::v4() ? "UDP/IPv4" : "UDP/IPv6") + "\n", MessageType::Info));

      return std::make_shared<SCUConnection>(this, std::move(handle), messageCallback);
   }

   void send(const std::vector<uint8_t>& msg) override {
      SN::UDPServer::send(msg);
      std::string text(msg.begin(), msg.end());
      messages.push_back(ChatMessage(text + "\n", MessageType::Server));
   }

   void disconnect() override {
      SN::UDPServer::disconnect();
   }
};

inline void SCUConnection::onRead(std::vector<uint8_t> msg) {
   std::string text(msg.begin(), msg.end());
   if(callback) (*callback)("message received from client: " + text + "\n", MessageType::Info);
   getServer<SCUServer>()->messages.push_back(ChatMessage(text + "\n", MessageType::Client));
}

inline void SCUConnection::onDisconnect() {
   if(callback) (*callback)("connection disconnected\n", MessageType::Info);
   getServer<SCUServer>()->messages.push_back(ChatMessage("connection disconnected\n", MessageType::Info));
}

inline void SCUConnection::sendAsConnection(const std::vector<uint8_t>& msg) {
   SN::UDPConnection::send(msg);
   std::string text(msg.begin(), msg.end());
   getServer<SCUServer>()->messages.push_back(ChatMessage(text + "\n", MessageType::Connection, id));
}

#endif // ~SIMPLECHAT_UDP_SERVER_H