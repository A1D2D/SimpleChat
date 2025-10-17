#include <VORTEX_MP/NestedLoops>
#include <iostream>
#include <string>
#include <vector>

#include "SRC/Networking_New/StreamedNet.h"
#include "SRC/Util/StringUtil.h"

enum ClientCommand {
   CC_Connect,
   CC_Disconnect,
   CC_Exit,
   CC_Message,
   CC_Test_F
};

class SimpleChatClient : public SNImpl::Client {
public:
   using SNImpl::Client::Client;

protected:
   void onResolve() override {
      std::cout << "resolve succesfull\n";
      connect();
   }

   void onConnect() override {
      std::cout << "connect succesfull\n";
      startRead();
   }
   
   void onRead() override {
      std::cout << "Server: ";
      while (!readQ.empty()) {
         std::cout << readQ.front();
         readQ.pop();
      }
      std::cout << "\n";
   }
};

int main(int argc, const char** argv) {
   std::string msg;
   std::vector<std::string> args;
   std::string errorMsg;
   ClientCommand cmd;

   std::unordered_map<std::string, ClientCommand> commands = {
      {"/connect", CC_Connect},
      {"/c", CC_Connect},
      {"/stop", CC_Disconnect},
      {"/d", CC_Disconnect},
      {"/e", CC_Exit},
      {"/exit", CC_Exit}
   };

   Colorb::SKY_BLUE.printAnsiStyle();
   std::cout << "SimpleChat\n";
   resetAnsiStyle();

   NestedLoop nl;

   std::shared_ptr<asio::io_context> context = std::make_shared<asio::io_context>();
   std::thread contextThread;

   SimpleChatClient client(context);
   contextThread = std::thread([context]() {
      context->run();
   });

   for (;;) {
      std::getline(std::cin, msg);
      args = StringUtil::split(msg, " ");
      if(args.empty()) continue;
      std::string cmdStr = args[0];
      std::shift_left(args.begin(), args.end(), 1);

      auto it = commands.find(cmdStr);
      if (it == commands.end()) {
         if(msg.size() >= 0 && msg[0] == '/') {
            std::cout << "Unknown command\n";
            continue;
         }
         cmd = CC_Message;
      } else {
         cmd = it->second;
      }
      switch (cmd) {
         case CC_Exit: {
            NL_BREAK(nl, 0);
         }
         case CC_Connect: {
            auto ip = StringUtil::parseArg<std::string>(args, 0);
            auto port = StringUtil::parseArg<ushort_16>(args, 1);
            if(!port || !ip) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }

            SNImpl::Client::printClient("Connecting to Server..", *ip, *port, true);
            client.resolve(*ip, *port);
            break;
         }
         case CC_Disconnect: {
            client.disconnect();
            break;
         }
         default: {
            SNImpl::Client::printClient(""+msg);
            client.send(StringUtil::stringToBytes(msg));
            break;
         }
      }
      NL_CHECK(nl,0);
   }

   if(contextThread.joinable()) contextThread.join();
   std::cout << "skipped" << std::endl;
   return 0;
}