#ifndef INSTANCE_MANNAGER_H
#define INSTANCE_MANNAGER_H

#include "Instance.h"
#include "StreamedNet.h"
#include "SimpleChatTCPClient.h"
#include "SimpleChatTCPServer.h"
#include "SimpleChatUDPClient.h"
#include "SimpleChatUDPServer.h"
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

template<typename T>
class InstanceManager {
public:
   InstanceManager() {
      use("0");
   }

   T& use(const std::string& id) {
      auto [it, inserted] = instances.emplace(id, T{});
      current = id;
      return it->second;
   }

   T& get() {
      return instances.at(current);
   }

   T& get(const std::string& id) {
      return instances.at(id);
   }

   bool remove(const std::string& id) {
      auto removed = instances.erase(id);
      if(!removed) return false;

      if(current == id || id == "0") use("0");
      return true;
   }

   std::string getCurrentSelectedId() {
      return current;
   }

   std::unordered_map<std::string, T> getInstances() const { return instances; }
   
private:
   std::unordered_map<std::string, T> instances;
   std::string current = "0";
};

class SimpleChatController : public InstanceManager<std::shared_ptr<Instance>> {
public:
   SimpleChatController() : guard(context), InstanceManager<std::shared_ptr<Instance>>() {
      th = std::thread([this]() {
         while(context.usage()) {
            context.poll();
         }
      });
   }

   void resolve(std::string ip, uint16_t port, bool tcp = true) {
      std::cout << "started resolver with " << (tcp ? "tcp" : "udp") << " protocol\n";
      auto resolver = std::make_shared<SCResolver>(context);
      if(tcp) {
         resolver->addResolveFunc([](std::vector<tcp::endpoint> endpoints){
            std::cout << "tcp Endpoints: \n";
            for (auto endpoint : endpoints) {
               std::cout << endpoint.address().to_string() << " : " << endpoint.port() << "\n";
            }
         });
         resolver->resolve<SN::NetworkMode::TCP>(ip, port);
      } else {
         resolver->addResolveFunc([](std::vector<udp::endpoint> endpoints){
            std::cout << "upd Endpoints: \n";
            for (auto endpoint : endpoints) {
               std::cout << endpoint.address().to_string() << " : " << endpoint.port() << "\n";
            }
         });
         resolver->resolve<SN::NetworkMode::UDP>(ip, port);
      }
   }

   void start(uint16_t port, bool tcp = true) {
      auto tcpV = tcp::v4();
      auto udpV = udp::v4();
      auto& instance = get();
      if(tcp) {
         if(!instance) instance = std::make_shared<SCTServer>(context);
         auto instPtr = std::dynamic_pointer_cast<SCTServer>(instance);
         if(!instPtr) return;
         instPtr->start(tcp::endpoint(tcpV, port));
      } else {
         if(!instance) instance = std::make_shared<SCUServer>(context);
         auto instPtr = std::dynamic_pointer_cast<SCUServer>(instance);
         if(!instPtr) return;
         instPtr->start(udp::endpoint(udpV, port));
      }
   }

   void connect(std::string ip, uint16_t port, bool tcp = true) {
      std::cout << "started connection resolve with " << (tcp ? "tcp" : "udp") << " protocol\n";
      auto resolver = std::make_shared<SCResolver>(context);
      if(tcp) {
         auto& instance = get();
         if(!instance) instance = std::make_shared<SCTClient>(context);

         resolver->addResolveFunc([this, instance](std::vector<tcp::endpoint> endpoints) {
            auto instPtr = std::dynamic_pointer_cast<SCTClient>(instance);
            if(!instPtr) return;
            instPtr->connect(endpoints);
         });
         resolver->resolve<SN::NetworkMode::TCP>(ip, port);
      } else {
         auto& instance = get();
         if(!instance) instance = std::make_shared<SCUClient>(context);

         resolver->addResolveFunc([this, instance](std::vector<udp::endpoint> endpoints) {
            auto instPtr = std::dynamic_pointer_cast<SCUClient>(instance);
            if(!instPtr) return;
            instPtr->connect(endpoints);
         });
         resolver->resolve<SN::NetworkMode::UDP>(ip, port);
      }
   }

   void stop() {
      auto instance = get();
      if(!instance) return;
      instance->disconnect();
   }
   
   void disconnect(int clientID) {
      auto instance = get();
      if(auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
         if(clientID > server->getConnections().size()) server->getConnections()[clientID]->disconnect();
      } else if(auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
         if(clientID > server->getConnections().size()) server->getConnections()[clientID]->disconnect();
      }
   }

   void disconnect() {
      auto instance = get();
      if(auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
         for (auto connection : server->getConnections()) {
            connection->disconnect();
         }
      } else if(auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
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
      if(auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
         if(clientID < server->getConnections().size()) server->getConnections()[clientID]->send(StringUtil::stringToBytes(str));
      } else if(auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
         if(clientID < server->getConnections().size()) server->getConnections()[clientID]->send(StringUtil::stringToBytes(str));
      }
   }
public:
   SN::Context context;
   SN::Context::UsageGuard guard;
   std::thread th;
};

#endif // ~INSTANCE_MANNAGER_H