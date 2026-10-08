#ifndef INSTANCE_CONTROLLER_H
#define INSTANCE_CONTROLLER_H

#include "Instance.h"
#include "SimpleChatTCPClient.h"
#include "SimpleChatTCPServer.h"
#include "SimpleChatUDPClient.h"
#include "SimpleChatUDPServer.h"
#include "StreamedNet.h"
#include "Util/StringUtil.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

class SCResolver : public SN::Resolver, public std::enable_shared_from_this<SCResolver> {
public:
   SCResolver(SN::Context context) : SN::Resolver(context) {}

   void addResolveFunc(std::function<void(std::vector<tcp::endpoint>)> func) {
      tcpFunc = func;
      self = shared_from_this();
   }

   void addResolveFunc(std::function<void(std::vector<udp::endpoint>)> func) {
      udpFunc = func;
      self = shared_from_this();
   }

   void onTcpResolve(std::vector<tcp::endpoint> resultEndpoints) override {
      tcpFunc(resultEndpoints);
      self.reset();
   }

   void onUdpResolve(std::vector<udp::endpoint> resultEndpoints) override {
      udpFunc(resultEndpoints);
      self.reset();
   }

   std::function<void(std::vector<tcp::endpoint>)> tcpFunc;
   std::function<void(std::vector<udp::endpoint>)> udpFunc;
   std::shared_ptr<SCResolver> self;
};

class InstanceManager {
public:
   InstanceManager() { 
      use("0");
   }

   std::shared_ptr<Instance>& use(const std::string& id) {
      auto [it, inserted] = instances.emplace(id, nullptr);
      current = id;
      return it->second;
   }

   std::shared_ptr<Instance>& get() { return instances.at(current); }

   std::shared_ptr<Instance>& get(const std::string& id) { return instances.at(id); }

   bool remove(const std::string& id) {
      auto removed = instances.erase(id);
      if (!removed) return false;

      if (current == id || id == "0") use("0");
      return true;
   }

   std::string getCurrentSelectedId() { return current; }

   std::unordered_map<std::string, std::shared_ptr<Instance>> getInstances() const { return instances; }

private:
   std::unordered_map<std::string, std::shared_ptr<Instance>> instances;
   std::string current = "0";
};

class SimpleChatController : public InstanceManager {
public:
   SimpleChatController(std::shared_ptr<MessageCallback> messageCallback) : guard(context), InstanceManager(), messageCallback(messageCallback) {
      // mutex = std::make_shared<std::mutex>();
      th = std::thread([this]() {
         while (context.usage()) {
            // std::lock_guard<std::mutex> guard(*mutex);
            context.poll();
         }
      });
   }

   void resolve(std::string ip, uint16_t port, bool tcp = true) {
      std::cout << "started resolver with " << (tcp ? "tcp" : "udp") << " protocol\n";
      auto resolver = std::make_shared<SCResolver>(context);
      if (tcp) {
         resolver->addResolveFunc([](std::vector<tcp::endpoint> endpoints) {
            std::cout << "tcp Endpoints: \n";
            for (auto endpoint : endpoints) {
               std::cout << endpoint.address().to_string() << " : " << endpoint.port() << "\n";
            }
         });
         resolver->resolve<SN::NetworkMode::TCP>(ip, port);
      } else {
         resolver->addResolveFunc([](std::vector<udp::endpoint> endpoints) {
            std::cout << "upd Endpoints: \n";
            for (auto endpoint : endpoints) {
               std::cout << endpoint.address().to_string() << " : " << endpoint.port() << "\n";
            }
         });
         resolver->resolve<SN::NetworkMode::UDP>(ip, port);
      }
   }

   void start(uint16_t port, bool tcp = true) {
      auto& instance = get();
      if (tcp) {
         if (!instance) instance = std::make_shared<SCTServer>(context, messageCallback);
         auto instPtr = std::dynamic_pointer_cast<SCTServer>(instance);
         if (!instPtr) return;
         instPtr->start({tcp::endpoint(tcp::v4(), port), tcp::endpoint(tcp::v6(), port)});
      } else {
         if (!instance) instance = std::make_shared<SCUServer>(context, messageCallback);
         auto instPtr = std::dynamic_pointer_cast<SCUServer>(instance);
         if (!instPtr) return;
         instPtr->start({udp::endpoint(udp::v4(), port), udp::endpoint(udp::v6(), port)});
      }
   }

   void connect(std::string ip, uint16_t port, bool tcp = true) {
      std::cout << "started connection resolve with " << (tcp ? "tcp" : "udp") << " protocol\n";
      auto resolver = std::make_shared<SCResolver>(context);
      if (tcp) {
         auto& instance = get();
         if (!instance) instance = std::make_shared<SCTClient>(context, messageCallback);

         resolver->addResolveFunc([this, instance](std::vector<tcp::endpoint> endpoints) {
            auto instPtr = std::dynamic_pointer_cast<SCTClient>(instance);
            if (!instPtr) return;
            instPtr->connect(endpoints);
         });
         resolver->resolve<SN::NetworkMode::TCP>(ip, port);
      } else {
         auto& instance = get();
         if (!instance) instance = std::make_shared<SCUClient>(context, messageCallback);

         resolver->addResolveFunc([this, instance](std::vector<udp::endpoint> endpoints) {
            auto instPtr = std::dynamic_pointer_cast<SCUClient>(instance);
            if (!instPtr) return;
            instPtr->connect(endpoints);
         });
         resolver->resolve<SN::NetworkMode::UDP>(ip, port);
      }
   }

   void stop() {
      auto instance = get();
      if (!instance) return;
      instance->disconnect();
   }

   void disconnect(int clientID) {
      auto instance = get();
      if (auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
         if (clientID < server->getConnections().size()) server->getConnections()[clientID]->disconnect();
      } else if (auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
         if (clientID < server->getConnections().size()) server->getConnections()[clientID]->disconnect();
      }
   }

   void disconnect() {
      auto instance = get();
      if (auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
         for (auto connection : server->getConnections()) {
            connection->disconnect();
         }
      } else if (auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
         for (auto connection : server->getConnections()) {
            connection->disconnect();
         }
      }
   }

   void send(std::string str) {
      auto instance = get();
      instance->send(StringUtil::stringToBytes(str));
   }

   void send(std::string str, int clientID) {
      auto instance = get();
      if (auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
         if (clientID < server->getConnections().size()) server->getConnections<SCTConnection>()[clientID]->sendAsConnection(StringUtil::stringToBytes(str));
      } else if (auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
         if (clientID < server->getConnections().size()) server->getConnections<SCUConnection>()[clientID]->sendAsConnection(StringUtil::stringToBytes(str));
      }
   }

   void printServerConnectionCount() {
      auto instance = get();
      if (auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
         std::cout << "connection count: " << server->getConnections().size() << "\n";
      } else if (auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
         std::cout << "connection count: " << server->getConnections().size() << "\n";
      }
   }

   std::vector<ChatMessage> getMessages() {
      auto instance = get();
      if(!instance) return {};
      return instance->messages;
   }

public:
   SN::Context context;
   SN::Context::UsageGuard guard;
   std::thread th;
   std::shared_ptr<MessageCallback> messageCallback;
};

#endif // INSTANCE_CONTROLLER_H