/*
#include "SRC/Util/NestedLoops.h"
#include <iostream>
#include <string>
#include <vector>

#include "SRC/Networking/PacketNet.h"
#include "SRC/Util/StringUtil.h"

enum ClientCommand {
   CC_Connect,
   CC_Disconnect,
   CC_Exit,
   CC_Message,
   CC_Test_F
};

class SimpleChatClient : public PN::Client<> {
public:
   using PN::Client<>::Client;

protected:
   void onResolve() override {
      std::cout << "resolve succesfull\n";
      connect();
   }

   void onConnect() override {
      std::cout << "connect succesfull\n";
      sendHandshake();
      startRead();
   }
   
   void onPacket(const PN::DefaultPacket& data) override {
      std::cout << "[Server]: " << StringUtil::bytesToString(data.data) << std::endl;
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

   // Colorb::SKY_BLUE.printAnsiStyle();
   std::cout << "SimpleChat: Packeted Client\n";
   // resetAnsiStyle();

   asio::io_context context;
   SimpleChatClient client(context);
   client.context.startThread();

   SN::NestedLoop nl;
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
            auto port = StringUtil::parseArg<uint16_t>(args, 1);
            if(!port || !ip) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }

            SN::Client::printClient("Connecting to Server..", *ip, *port, true);
            client.resolve(*ip, *port);
            break;
         }
         case CC_Disconnect: {
            client.disconnect();
            break;
         }
         default: {
            SN::Client::printClient(""+msg);
            client.sendPacket(StringUtil::stringToBytes(msg));
            break;
         }
      }
      NL_CHECK(nl,0);
   }

   client.context.stopThread();
   std::cout << "skipped" << std::endl;
   return 0;
}
/**/

int main() {
   return 0;
}