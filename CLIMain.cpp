#include "SRC/Controller.h"
#include "SRC/CLIInterpreter.h"
#include "SRC/Instance.h"

#include <memory>

int main() {
   std::cout << "SimpleChat CLI\n";

   

   auto callback = std::make_shared<MessageCallback>([](std::string message, MessageType type){
      switch (type) {
         case MessageType::Info:
            std::cout << message;
            break;
         case MessageType::Warning:
            std::cerr << message;
            break;
         default:
            std::cerr << "invalid type\n";
      }
   });

   auto controller = std::make_shared<SimpleChatController>(callback);
   CliInterpreter interpreter(callback, controller);

   bool breakWhile = false;
   while(true) {
      std::string msg;
      std::getline(std::cin, msg);
      interpreter.parseCommandMessage(msg, breakWhile);
      if(breakWhile) break;
   }

   std::cout << "exited" << std::endl;
   return 0;
};