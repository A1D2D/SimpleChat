#include "../SRC/Util/StringUtil.h"
#include <StreamedNet.h>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <chrono>

using namespace SN;

TEST_CASE("Context starts with zero usage") {
   SN::Context context;

   REQUIRE(context.usage() == 0);
}

TEST_CASE("Context use and release") {
   SN::Context context;

   context.use();
   REQUIRE(context.usage() == 1);

   context.use();
   REQUIRE(context.usage() == 2);

   context.release();
   REQUIRE(context.usage() == 1);

   context.release();
   REQUIRE(context.usage() == 0);
}

TEST_CASE("Copied Context shares state") {
   SN::Context context1;
   SN::Context context2 = context1;

   context1.use();

   REQUIRE(context1.usage() == 1);
   REQUIRE(context2.usage() == 1);

   context2.use();

   REQUIRE(context1.usage() == 2);
   REQUIRE(context2.usage() == 2);
}

TEST_CASE("UsageGuard releases automatically") {
   SN::Context context;

   REQUIRE(context.usage() == 0);

   {
      SN::Context::UsageGuard guard(context);

      REQUIRE(context.usage() == 1);
   }

   REQUIRE(context.usage() == 0);
}

TEST_CASE("UsageGuard move construction") {
   SN::Context context;

   {
      SN::Context::UsageGuard guard1(context);

      REQUIRE(context.usage() == 1);

      SN::Context::UsageGuard guard2(std::move(guard1));

      REQUIRE(context.usage() == 1);
   }

   REQUIRE(context.usage() == 0);
}

TEST_CASE("UsageGuard manual release") {
   SN::Context context;

   {
      SN::Context::UsageGuard guard(context);

      REQUIRE(context.usage() == 1);

      guard.release();

      REQUIRE(context.usage() == 0);
   }

   REQUIRE(context.usage() == 0);
}

TEST_CASE("Context callback can be added") {
   SN::Context context;

   auto handle = context.addCallback([]() {
      });

   REQUIRE_FALSE(handle.expired());
}

TEST_CASE("Context callback can be removed") {
   SN::Context context;

   auto handle = context.addCallback([]() {
      });

   REQUIRE_FALSE(handle.expired());

   context.removeCallback(handle);

   REQUIRE(handle.expired());
}

class TestClient : public Client<NetworkMode::TCP> {
public:
   bool connected = false;
   bool disconnected = false;
   bool received = false;
   bool written = false;
   std::vector<uint8_t> receivedData;

   TestClient(Context context) : Client(context) {}

   void onConnect() override {
      startRead();
      connected = true;
   }

   void onRead(std::vector<uint8_t> msg) override {
      received = true;
      receivedData = std::move(msg);
   }

   void onWrite() override {
      written = true;
   }

   void onDisconnect() override {
      disconnected = true;
   }
};

class TestConnection : public Connection<NetworkMode::TCP> {
public:
   bool started = false;
   bool received = false;
   bool written = false;
   bool disconnected = false;

   std::vector<uint8_t> receivedData;

   TestConnection(Server<NetworkMode::TCP>* server, tcp::socket socket) : Connection(server, std::move(socket)) {}

   void onStart() override {
      started = true;
      startRead();
   }

   void onRead(std::vector<uint8_t> msg) override {
      received = true;
      receivedData = std::move(msg);
   }

   void onWrite() override {
      written = true;
   }

   void onDisconnect() override {
      disconnected = true;
   }
};

class TestServer : public Server<NetworkMode::TCP> {
public:
   bool started = false;
   std::shared_ptr<Connection<NetworkMode::TCP>> connection;
   bool clientConnected = false;

   TestServer(Context context) : Server(context) {}

   void onStart() override {
      startAccept();
      started = true;
   }

   std::shared_ptr<Connection<NetworkMode::TCP>> onAccept(tcp::socket acceptedSocket) override {
      auto connection = std::make_shared<TestConnection>(this, std::move(acceptedSocket));
      this->connection = connection;
      clientConnected = true;
      return connection;
   }
};

class TestUDPClient : public Client<NetworkMode::UDP> {
public:
   bool connected = false;
   bool received = false;
   std::vector<uint8_t> receivedData;

   TestUDPClient(Context context) : Client(context) {}

   void onConnect() override { 
      connected = true; 
   }
   void onRead(std::vector<uint8_t> msg) override {
      received = true;
      receivedData = std::move(msg);
   }
};

class TestUDPConnection : public Connection<NetworkMode::UDP> {
public:
   bool started = false;
   bool received = false;
   std::vector<uint8_t> receivedData;

   TestUDPConnection(Server<NetworkMode::UDP>* server, UDPServer::UdpHandle handle)
      : Connection(server, std::move(handle)) {
   }

   void onStart() override { started = true; }
   void onRead(std::vector<uint8_t> msg) override {
      received = true;
      receivedData = std::move(msg);
   }
};

class TestUDPServer : public Server<NetworkMode::UDP> {
public:
   bool started = false;
   bool clientConnected = false;
   std::shared_ptr<TestUDPConnection> lastConnection;

   TestUDPServer(Context context) : Server(context) {}

   void onStart() override {
      startRead();
      started = true;
   }

   std::shared_ptr<Connection<NetworkMode::UDP>> onAccept(UdpHandle handle, std::vector<uint8_t> msg) override {
      auto connection = std::make_shared<TestUDPConnection>(this, std::move(handle));
      lastConnection = connection;
      clientConnected = true;

      // If UDP connect includes an initial message, capture it
      if (!msg.empty()) {
         connection->onRead(std::move(msg));
      }
      return connection;
   }
};

TEST_CASE("TCP client sends data to server") {
   bool timedOut = false;
   auto timeOut = std::chrono::seconds(5);

   Context context;

   TestServer server(context);
   TestClient client(context);

   const uint16_t port = 45679;
   auto endpoint = tcp::endpoint(asio::ip::make_address("127.0.0.1"), port);

   server.start(endpoint);

   auto start = std::chrono::steady_clock::now();
   client.connect(endpoint);

   start = std::chrono::steady_clock::now();
   while (!(client.connected && server.clientConnected)) {
      auto elapsed = std::chrono::steady_clock::now() - start;

      if (elapsed >= timeOut) { timedOut = true; break; }

      context.poll();
   }
   REQUIRE_FALSE(timedOut);

   REQUIRE(client.connected);
   REQUIRE(server.clientConnected);
   REQUIRE(server.getConnections().size() == 1);

   auto connection = server.getConnections<TestConnection>()[0];
   REQUIRE(connection != nullptr);

   std::vector<uint8_t> message = StringUtil::stringToBytes("Test Text");

   client.send(message);

   start = std::chrono::steady_clock::now();
   while (connection->receivedData.size() <= 0) {
      auto elapsed = std::chrono::steady_clock::now() - start;

      if (elapsed >= timeOut) { timedOut = true; break; }

      context.poll();
   }
   REQUIRE_FALSE(timedOut);

   REQUIRE(connection->receivedData.size() > 0);
   REQUIRE(StringUtil::bytesToString(connection->receivedData) == "Test Text");

   client.shutdown();
   server.shutdown();
}

TEST_CASE("TCP Server handles multiple clients") {
   bool timedOut = false;
   auto timeOut = std::chrono::seconds(5);
   Context context;
   TestServer server(context);

   const uint16_t port = 45680;
   auto endpoint = tcp::endpoint(asio::ip::make_address("127.0.0.1"), port);
   server.start(endpoint);

   const int numClients = 3;
   std::vector<std::unique_ptr<TestClient>> clients;
   for (int i = 0; i < numClients; i++) {
      clients.push_back(std::make_unique<TestClient>(context));
      clients.back()->connect(endpoint);
   }

   auto start = std::chrono::steady_clock::now();
   while (server.getConnections().size() < numClients) {
      if (std::chrono::steady_clock::now() - start >= timeOut) { timedOut = true; break; }
      context.poll();
   }

   REQUIRE_FALSE(timedOut);
   REQUIRE(server.getConnections().size() == numClients);

   for (auto& client : clients) {
      client->shutdown();
   }
   server.shutdown();
}

TEST_CASE("TCP Client and Server disconnection") {
   bool timedOut = false;
   auto timeOut = std::chrono::seconds(5);
   Context context;
   TestServer server(context);
   TestClient client(context);

   const uint16_t port = 45681;
   auto endpoint = tcp::endpoint(asio::ip::make_address("127.0.0.1"), port);
   server.start(endpoint);
   client.connect(endpoint);

   auto start = std::chrono::steady_clock::now();
   while (!(client.connected && server.clientConnected)) {
      if (std::chrono::steady_clock::now() - start >= timeOut) { timedOut = true; break; }
      context.poll();
   }

   REQUIRE(server.getConnections().size() == 1);
   auto connection = server.getConnections<TestConnection>()[0];

   // Trigger disconnect
   client.disconnect();

   start = std::chrono::steady_clock::now();
   while (!client.disconnected || !connection->disconnected) {
      if (std::chrono::steady_clock::now() - start >= timeOut) { timedOut = true; break; }
      context.poll();
   }

   REQUIRE_FALSE(timedOut);
   REQUIRE(client.disconnected);
   REQUIRE(connection->disconnected);
}

TEST_CASE("Client move semantics maintain connection state") {
   bool timedOut = false;
   auto timeOut = std::chrono::seconds(5);
   Context context;
   TestServer server(context);
   TestClient client1(context);

   const uint16_t port = 45682;
   auto endpoint = tcp::endpoint(asio::ip::make_address("127.0.0.1"), port);
   server.start(endpoint);
   client1.connect(endpoint);

   // Move the client while connected[cite: 1]
   TestClient client2 = std::move(client1);

   // Wait for connection
   auto start = std::chrono::steady_clock::now();
   while (!(client2.connected && server.clientConnected)) {
      if (std::chrono::steady_clock::now() - start >= timeOut) { timedOut = true; break; }
      context.poll();
   }

   std::vector<uint8_t> message = StringUtil::stringToBytes("Moved Data");
   client2.send(message);

   auto connection = server.getConnections<TestConnection>()[0];

   start = std::chrono::steady_clock::now();
   while (connection->receivedData.empty()) {
      if (std::chrono::steady_clock::now() - start >= timeOut) { timedOut = true; break; }
      context.poll();
   }

   REQUIRE_FALSE(timedOut);
   REQUIRE(StringUtil::bytesToString(connection->receivedData) == "Moved Data");
}

TEST_CASE("UDP client sends data to server") {
   bool timedOut = false;
   auto timeOut = std::chrono::seconds(5);
   Context context;
   TestUDPServer server(context);
   TestUDPClient client(context);

   const uint16_t port = 45683;
   auto endpoint = udp::endpoint(asio::ip::make_address("127.0.0.1"), port);
   server.start(endpoint);

   // UDP is connectionless, so we send data to initialize the server's tracking of the client
   client.connect(endpoint);
   std::vector<uint8_t> message = StringUtil::stringToBytes("UDP Test Message");
   client.send(message);

   auto start = std::chrono::steady_clock::now();
   while (!server.clientConnected || !server.lastConnection->received) {
      if (std::chrono::steady_clock::now() - start >= timeOut) { timedOut = true; break; }
      context.poll();
   }

   REQUIRE_FALSE(timedOut);
   REQUIRE(server.clientConnected);
   REQUIRE(server.lastConnection != nullptr);
   REQUIRE(StringUtil::bytesToString(server.lastConnection->receivedData) == "UDP Test Message");
}

TEST_CASE("TCP Server handles interleaved connects, disconnects, kicks, and data transfers") {
   auto timeOut = std::chrono::seconds(10);
   Context context;
   TestServer server(context);

   const uint16_t port = 45685;
   auto endpoint = tcp::endpoint(asio::ip::make_address("127.0.0.1"), port);
   server.start(endpoint);

   // Helper lambda to cleanly poll the context until a condition is met or it times out
   auto waitFor = [&](std::function<bool()> condition) {
      auto start = std::chrono::steady_clock::now();
      while (!condition()) {
         if (std::chrono::steady_clock::now() - start >= timeOut) return false;
         context.poll();
      }
      return true;
   };

   TestClient clientA(context);
   TestClient clientB(context);
   TestClient clientC(context);

   // 1. Client A and B connect simultaneously
   clientA.connect(endpoint);
   clientB.connect(endpoint);
   REQUIRE(waitFor([&]() { return server.getConnections().size() == 2; }));
   REQUIRE(clientA.connected);
   REQUIRE(clientB.connected);

   // 2. Client A sends data
   clientA.send(StringUtil::stringToBytes("Msg from A"));
   REQUIRE(waitFor([&]() {
      for (auto& c : server.getConnections<TestConnection>()) {
         if (c->received && StringUtil::bytesToString(c->receivedData) == "Msg from A") return true;
      }
      return false;
      }));

   // 3. Client A disconnects itself WHILE Client C is connecting
   clientA.disconnect();
   clientC.connect(endpoint);

   // Verify A drops and C establishes, leaving exactly 2 active connections (B and C)
   REQUIRE(waitFor([&]() { return clientA.disconnected; }));
   REQUIRE(waitFor([&]() { return clientC.connected; }));
   REQUIRE(waitFor([&]() { return server.getConnections().size() == 2; }));

   // 4. Clients B and C send data so we can identify their server-side connection objects
   clientB.send(StringUtil::stringToBytes("Msg from B"));
   clientC.send(StringUtil::stringToBytes("Msg from C"));

   REQUIRE(waitFor([&]() {
      int receivedCount = 0;
      for (auto& c : server.getConnections<TestConnection>()) {
         std::string msg = StringUtil::bytesToString(c->receivedData);
         if (msg == "Msg from B" || msg == "Msg from C") receivedCount++;
      }
      return receivedCount == 2;
   }));

   // 5. Server forcefully kicks Client B based on its received message
   for (auto& c : server.getConnections<TestConnection>()) {
      if (StringUtil::bytesToString(c->receivedData) == "Msg from B") {
         c->disconnect();
      }
   }

   // Verify B is kicked and only C remains
   REQUIRE(waitFor([&]() { return clientB.disconnected; }));
   REQUIRE(waitFor([&]() { return server.getConnections().size() == 1; }));

   // 6. Client C sends one final message to ensure the server is still processing reads
   clientC.send(StringUtil::stringToBytes("Final Msg from C"));
   REQUIRE(waitFor([&]() {
      auto conns = server.getConnections<TestConnection>();
      if (conns.empty()) return false;
      return StringUtil::bytesToString(conns[0]->receivedData) == "Final Msg from C";
   }));

   clientC.shutdown();
   server.shutdown();
}