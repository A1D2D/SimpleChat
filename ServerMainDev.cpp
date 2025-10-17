#include <iostream>

#include <VORTEX_MP/NestedLoops>

#include "SRC/Util/StringUtil.h"
#include "SRC/Networking_New/StreamedNet.h"

enum ServerCommand {
   SC_StartServer,
   SC_StopServer,
   SC_Exit,
   SC_Message
};

class SimpleChatConnection : public SNImpl::Connection {
public:
   using SNImpl::Connection::Connection;

protected:
   void onConnect() override {
      std::cout << "connection recived\n";
   }

   void onStart() override {
      std::cout << "connection started\n";
      startRead();
   }

   void onRead() override {
      std::cout << "Client: ";
      while (!readQ.empty()) {
         std::cout << readQ.front();
         readQ.pop();
      }
      std::cout << "\n";
   }
};

class SimpleChatServer : public SNImpl::Server {
public:
   using SNImpl::Server::Server;

protected:
   void onStart() override {
      printServer("started", getPort(), true);
      startAccept();
   }

   std::shared_ptr<SNImpl::Connection> onAccept(tcp::socket& socket) override {
      printServer("client Accepted");
      return std::make_shared<SimpleChatConnection>(this->getContext(), *this, socket);
   }

   //void onDisconnect(std::shared_ptr<PN::PacketNetConnection<>> connection) override {
   //   printServer("client disconnected", getPort(), true);
   //}
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

   
   std::shared_ptr<asio::io_context> context = std::make_shared<asio::io_context>();
   std::thread contextThread;

   SimpleChatServer server(context);
   contextThread = std::thread([context]() {
      context->run();
   });

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
            SimpleChatServer::printServer("Server Created..", server.getPort(), true);
            break;
         }
         case SC_StopServer: {
            server.close();
            SimpleChatServer::printServer("server closed");
            break;
         }
         default: {
            SimpleChatServer::printServer(""+msg);
            asio::post(*server.getContext(), [&, msg]() {
               for(auto& connection : server.getConnections()) {
                  connection->send(StringUtil::stringToBytes(msg));
               }
            });
            break;
         }
      }
      NL_CHECK(nl, 0);
   }

   if(contextThread.joinable()) contextThread.join();
   std::cout << "skipped" << std::endl;

   return 0;
}