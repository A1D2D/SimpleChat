#include <VORTEX_MP/NestedLoops>
#include <iostream>
#include <string>

#include "SRC/Util/StringUtil.h"

// #include "SRC/Client/Client.h"
#include "SRC/ServerClient/StreamedNet.h"

enum ClientCommand {
   CC_Connect,
   CC_Disconnect,
   CC_Exit,
   CC_Message,
   CC_Test_F
};

class SimpleChatClient : public SN::StreamedNetClient {
protected:
   void onReceive(const std::vector<ubyte_8>& data) override {
      std::cout << "[Server]: " << StringUtil::bytesToString(data) << std::endl;
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

   SimpleChatClient client;

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

            SN::StreamedNetClient::printClient("Connecting to Server..", *ip, *port, true);
            client.autoConnect(*ip, *port);
            break;
         }
         case CC_Disconnect: {
            client.disconnect();
            break;
         }
         default: {
            SN::StreamedNetClient::printClient(""+msg);
            client.send(msg);
            break;
         }
      }
      NL_CHECK(nl,0);
   }
   std::cout << "skipped" << std::endl;
   client.joinThread();
   
   return 0;
}