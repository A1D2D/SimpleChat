#include <VORTEX_MP/NestedLoops>
#include <iostream>
#include <string>

#include "SRC/Util/StringUtil.h"

#include "SRC/Client/Client.h"

enum ClientCommand {
   CC_Connect,
   CC_Disconnect,
   CC_Exit,
   CC_Message
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

   SimpleClient client;

   NestedLoop nl;
   for (;;) {
      std::getline(std::cin, msg);
      args = StringUtil::split(msg, " ");
      if(args.empty()) continue;
      std::string cmdStr = args[0];
      std::shift_left(args.begin(), args.end(), 1);

      auto it = commands.find(cmdStr);
      if (it == commands.end()) {
         // std::cout << "Unknown command\n";
         // continue;
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

            SimpleClient::printClient("Connecting to Server..", *ip, *port, true);
            client.connect(*ip, *port);
            client.startThread();
            break;
         }
         case CC_Disconnect: {
            SimpleClient::printClient("client disconnected");
            break;
         }
         default: {
            SimpleClient::printClient("msg: " + msg);
            client.send(msg);
            break;
         }
      }
      NL_CHECK(nl, 0);
   }
   return 0;
}

/*
   ushort_16 port = 12345;
   asio::io_context io;

   SimpleClient client(io, "127.0.0.1", port);
   std::cout << "SimpleChat ";
   Colorb::SKY_BLUE.printAnsiStyle();
   std::cout << "[Client: " << port << "]\n";
   resetAnsiStyle();

   std::thread ioThread([&]() { io.run(); });

   std::string line;
   while (std::getline(std::cin, line)) {
      if (line == "/exit") break;
      client.send(line);
   }

   io.stop();
   ioThread.join();
*/