#ifndef SIMPLECHAT_INSTANCE_H
#define SIMPLECHAT_INSTANCE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <iostream>

enum class MessageType {
   Info,
   Warning,
   Server,
   Client,
   Connection
};

struct ChatMessage {
   ChatMessage(std::string message, MessageType type, int id = 0) : id(id), message(message), type(type) {}
   std::string message;
   MessageType type;
   int id;
};

using MessageCallback = std::function<void(std::string, MessageType)>;

class Instance {
public:
   Instance(std::shared_ptr<MessageCallback> messageCallback) : messageCallback(messageCallback) {}
   std::vector<ChatMessage> messages;
   std::shared_ptr<MessageCallback> messageCallback;

   virtual ~Instance() = default;
   
   virtual void send(const std::vector<uint8_t>& msg) {
      std::cerr << "undefined object sent failed\n";
   }

   virtual void disconnect() {
      std::cerr << "undefined object cant be disconnected\n";
   }
};

#endif // ~INSTANCE_H