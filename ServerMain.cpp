#include <iostream>

#include <VORTEX_MP/NestedLoops>

#include "SRC/Util/StringUtil.h"
#include "SRC/Networking/StreamedNet.h"

enum ServerCommand {
   SC_StartServer,
   SC_StopServer,
   SC_Exit,
   SC_Message
};

class SimpleChatConnection : public SN::StreamedNetConnection {
public:
   using SN::StreamedNetConnection::StreamedNetConnection;
protected:
   void onReceive(const std::vector<ubyte_8>& data) override {
      std::cout << "[Client]: " << StringUtil::bytesToString(data) << "\n";

      std::string msg = StringUtil::bytesToString(data);

      if(StringUtil::containsAny(msg, {"labda", "kacsa", "idk"})) {
         disconnect();
         return;
      }

      asio::post(getContext(), [&, data]() {
         for (auto& connection : getServer().getConnections()) {
            if (connection.get() == this) continue;
            connection->send(data);
         }
      });
   }

   void onEvent(Event evt) override {}
};

class SimpleChatServer : public SN::StreamedNetServer {
protected:
   void onStart() override {
      printServer("started", getPort(), true);
   }

   std::shared_ptr<SN::StreamedNetConnection> onAccept(tcp::socket &socket) override {
      printServer("client Accepted", getPort(), true);
      return std::make_shared<SimpleChatConnection>(this->getContext(),*this, socket);
   }

   void onDisconnect(std::shared_ptr<SN::StreamedNetConnection> connection) override {
      printServer("client disconnected", getPort(), true);
   }
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

   SimpleChatServer server;

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

            server.start(*port);
            SN::StreamedNetServer::printServer("Server Created..", server.getPort(), true);
            break;
         }
         case SC_StopServer: {
            server.close();
            SN::StreamedNetServer::printServer("server closed");
            break;
         }
         default: {
            SN::StreamedNetServer::printServer(""+msg);
            asio::post(server.getContext(), [&, msg]() {
               for(auto& connection : server.getConnections()) {
                  connection->send(msg);
               }
            });
            break;
         }
      }
      NL_CHECK(nl, 0);
   }
   return 0;
}
