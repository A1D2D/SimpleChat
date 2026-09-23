#ifndef SIMPLECHAT_TCP_SERVER_H
#define SIMPLECHAT_TCP_SERVER_H

#include <StreamedNet.h>
#include "Instance.h"

class SCTConnection : public SN::TCPConnection {
public:
   SCTConnection(SN::TCPServer* server, tcp::socket socket) : SN::TCPConnection(server, std::move(socket)) {}

   void onRead(std::vector<uint8_t> msg) override {
      std::string text(msg.begin(), msg.end());
      std::cout << "message received from client" << ": " << text << "\n";
   }
};

class SCTServer : public SN::TCPServer, public Instance {
public:
   SCTServer(SN::Context context) : SN::TCPServer(context) {}

   void onStart() override {
      std::cout << "start accept\n";
      startAccept();
   }

   std::shared_ptr<SN::TCPConnection> onAccept(tcp::socket socket) override {
      std::error_code ec;

      std::cout << "=== Socket ===\n";
      std::cout << "Open: " << socket.is_open() << '\n';
      std::cout << "Native handle: " << socket.native_handle() << '\n';

      auto local = socket.local_endpoint(ec);
      if (!ec) std::cout << "Local: " << local.address().to_string() << ':' << local.port() << '\n';
      ec.clear();

      auto remote = socket.remote_endpoint(ec);
      if (!ec) std::cout << "Remote: " << remote.address().to_string() << ':' << remote.port() << '\n';
      
      std::cout << "Protocol: " << (socket.local_endpoint().protocol() == tcp::v4() ? "TCP/IPv4" : "TCP/IPv6") << '\n';
      return std::make_shared<SCTConnection>(this, std::move(socket));
   }

   void send(const std::vector<uint8_t>& msg) override {
      SN::TCPServer::send(msg);
   }

   void disconnect() override {
      SN::TCPServer::disconnect();
   }
};

#endif // ~SIMPLECHAT_TCP_SERVER_H