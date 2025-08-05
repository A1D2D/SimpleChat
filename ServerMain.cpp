#include <VORTEX_MP/NestedLoops>
#include <iostream>

#include "SRC/Util/StringUtil.h"

#include "SRC/Server/Server.h"

enum ServerCommand {
   SC_StartServer,
   SC_StopServer,
   SC_Exit,
   SC_Message
};

int main(int argc, const char** argv) {
   std::string msg;
   std::vector<std::string> args;
   std::string errorMsg;
   ServerCommand cmd;

   std::unordered_map<std::string, ServerCommand> commands = {
      {"/server", SC_StartServer},
      {"/s", SC_StartServer},
      {"/stop", SC_StopServer},
      {"/d", SC_StopServer},
      {"/e", SC_Exit},
      {"/exit", SC_Exit}
   };

   Colorb::BRONZE.printAnsiStyle();
   std::cout << "SimpleChat\n";
   resetAnsiStyle();

   SimpleServer server;

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
         cmd = SC_Message;
      } else {
         cmd = it->second;
      }

      switch (cmd) {
         case SC_Exit: {
            NL_BREAK(nl, 0);
         }
         case SC_StartServer: {
            auto port = StringUtil::parseArg<ushort_16>(args, 0);
            if(!port) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }

            SimpleServer::printServer("Server Created..", *port, true);
            server.start(*port);
            server.startThread();
            break;
         }
         case SC_StopServer: {
            SimpleServer::printServer("server closed");
            break;
         }
         default: {
            SimpleServer::printServer("msg: " + msg);
            for(auto& connection : server.sockets) {
               connection->send(msg);
            }
            break;
         }
      }
      NL_CHECK(nl, 0);
   }
   return 0;
}
