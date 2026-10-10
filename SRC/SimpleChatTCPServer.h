#ifndef SIMPLECHAT_TCP_SERVER_H
#define SIMPLECHAT_TCP_SERVER_H

#include <StreamedNet.h>
#include <string>
#include "Instance.h"
#include "StreamedNetCore.h"

class SCTServer;

class SCTConnection : public SN::TCPConnection {
public:
   SCTConnection(SN::TCPServer* server, tcp::socket socket, std::shared_ptr<MessageCallback> callback, int id) : SN::TCPConnection(server, std::move(socket)), callback(callback), id(id) {}

   void onRead(std::vector<uint8_t> msg) override;

   void onDisconnect() override;

   void sendAsConnection(const std::vector<uint8_t>& msg);

   std::shared_ptr<MessageCallback> callback;
   int id;
};

class SCTServer : public SN::TCPServer, public Instance {
public:
   SCTServer(SN::Context context, std::shared_ptr<MessageCallback> callback) : SN::TCPServer(context), Instance(callback) {}

   void onStart() override {
      if(messageCallback) (*messageCallback)("start accept\n", MessageType::Info);
      messages.push_back(ChatMessage("start accept\n", MessageType::Info));
      startAccept();
   }

   std::shared_ptr<SN::TCPConnection> onAccept(tcp::socket socket) override {
      std::error_code ec;

      if(messageCallback) (*messageCallback)("=== Socket ===\n", MessageType::Info);
      if(messageCallback) (*messageCallback)(std::string("Open: ") + (socket.is_open() ? "true" : "false") + "\n", MessageType::Info);
      if(messageCallback) (*messageCallback)("Native handle: " + std::to_string(socket.native_handle()) + "\n", MessageType::Info);
      
      messages.push_back(ChatMessage("=== Socket ===\n", MessageType::Info));
      messages.push_back(ChatMessage(std::string("Open: ") + (socket.is_open() ? "true" : "false") + "\n", MessageType::Info));
      messages.push_back(ChatMessage("Native handle: " + std::to_string(socket.native_handle()) + "\n", MessageType::Info));
      
      auto local = socket.local_endpoint(ec);
      if (!ec && messageCallback) (*messageCallback)("Local: " + local.address().to_string() + ":" + std::to_string(local.port()) + "\n", MessageType::Info);
      if (!ec) messages.push_back(ChatMessage("Local: " + local.address().to_string() + ":" + std::to_string(local.port()) + "\n", MessageType::Info));
      ec.clear();

      auto remote = socket.remote_endpoint(ec);
      if (!ec && messageCallback) (*messageCallback)("Remote: " + remote.address().to_string() + ":" + std::to_string(remote.port()) + "\n", MessageType::Info);
      if (!ec) messages.push_back(ChatMessage("Remote: " + remote.address().to_string() + ":" + std::to_string(remote.port()) + "\n", MessageType::Info));
      
      if(messageCallback) (*messageCallback)(std::string(socket.local_endpoint().protocol() == tcp::v4() ? "TCP/IPv4" : "TCP/IPv6") + "\n", MessageType::Info);
      messages.push_back(ChatMessage(std::string(socket.local_endpoint().protocol() == tcp::v4() ? "TCP/IPv4" : "TCP/IPv6") + "\n", MessageType::Info));
      
      return std::make_shared<SCTConnection>(this, std::move(socket), messageCallback, 0);
   }

   void send(const std::vector<uint8_t>& msg) override {
      SN::TCPServer::send(msg);
      std::string text(msg.begin(), msg.end());
      messages.push_back(ChatMessage(text + "\n", MessageType::Server));
      if(messageCallback) (*messageCallback)("sending:" + text + "\n", MessageType::Info);
   }

   void disconnect() override {
      SN::TCPServer::disconnect();
   }
};

inline void SCTConnection::onRead(std::vector<uint8_t> msg) {
   std::string text(msg.begin(), msg.end());
   if(callback) (*callback)("message received from client: " + text + "\n", MessageType::Info);
   getServer<SCTServer>()->messages.push_back(ChatMessage(text + "\n", MessageType::Client));
}

inline void SCTConnection::onDisconnect() {
   if(callback) (*callback)("connection disconnected\n", MessageType::Info);
   getServer<SCTServer>()->messages.push_back(ChatMessage("connection disconnected\n", MessageType::Info));
}

inline void SCTConnection::sendAsConnection(const std::vector<uint8_t>& msg) {
   SN::TCPConnection::send(msg);
   std::string text(msg.begin(), msg.end());
   getServer<SCTServer>()->messages.push_back(ChatMessage(text + "\n", MessageType::Connection, id));
   if(callback) (*callback)("sending:" + text + "\n", MessageType::Info);
};

#endif // ~SIMPLECHAT_TCP_SERVER_H