#include <iostream>

#include "SRC/Util/NestedLoops.h"

#include "SRC/Util/StringUtil.h"
#include "SRC/Networking/StreamedNet.h"

enum PeerCommand {
   PC_StartPeer,
   PC_StopPeer,
   PC_Exit,
   PC_Message,
   PC_ReqEndpointCount,
   PC_Add,
   PC_Remove,
   PC_ID,
   PC_Help
};

class SimpleChatServer;

class SimpleChatConnection : public SN::Connection {
public:
   using SN::Connection::Connection;

protected:
   void onConnect() override {
      std::cout << "connection recived\n";
   }

   void onStart() override {
      std::cout << "connection started\n";
      startRead();
   }

   void onRead() override;
};

class SimpleChatServer : public SN::Server {
public:
   SimpleChatServer(int serverID_) : SN::Server::Server(), severID(serverID_) {}
   // using SN::Server::Server;

   int severID = 0;
protected:
   void onStart() override {
      printServer("started", getPort(), true);
      startAccept();
   }

   std::shared_ptr<SN::Connection> onAccept(tcp::socket& socket) override {
      printServer("client Accepted");
      return std::make_shared<SimpleChatConnection>(this->getControllerClone(), this, socket);
   }

   void onDisconnect(std::shared_ptr<SN::Connection> connection) override {
      printServer("client disconnected", getPort(), true);
   }
};

void SimpleChatConnection::onRead() {
   std::cout << "SID: " << getServer<SimpleChatServer>()->severID << ", Client: ";
   while (!readQ.empty()) {
      std::cout << readQ.front();
      readQ.pop();
   }
   std::cout << "\n";
}

int main() {
   std::string msg;
   std::vector<std::string> args;
   std::string errorMsg;
   PeerCommand cmd;
   int cSId = 0;

   std::unordered_map<std::string, PeerCommand> commands = {
      {"/server", PC_StartPeer},
      {"/s", PC_StartPeer},
      {"/stop", PC_StopPeer},
      {"/d", PC_StopPeer},
      {"/e", PC_Exit},
      {"/exit", PC_Exit},
      {"/rcc", PC_ReqEndpointCount},
      {"/add", PC_Add},
      {"/remove", PC_Remove},
      {"/i", PC_ID},
      {"/h", PC_Help}
   };

   // Colorb::BRONZE.printAnsiStyle();
   std::cout << "SimpleChat: Server\n";
   // resetAnsiStyle();
   
   {
      std::vector<SimpleChatServer> servers;
      servers.emplace_back(std::move(SimpleChatServer(servers.size())));

      SN::NestedLoop nl;
      for (;;) {
         std::getline(std::cin, msg);
         args = StringUtil::split(msg, " ");
         if(args.empty()) continue;
         std::string cmdStr = args[0];
         shift_left(args.begin(), args.end(), 1);

         auto it = commands.find(cmdStr);
         if (it == commands.end()) {
            // std::cout << "Unknown command\n";
            // continue;
            cmd = PC_Message;
         } else {
            cmd = it->second;
         }
         if(servers.size() <= 0 && (cmd != PC_Add && cmd != PC_Help)) {
            std::cout << "server must be added before using any other command\n";
            continue;
         }
         switch (cmd) {
            case PC_Help: {
               std::cout <<
               "Available Commands:""\n""\n"
               "/server, /s""\n"
               "    Start the server. argument: [port] to specify the server port.""\n"
               "    Example: /server 8080""\n""\n"
               "/stop, /d""\n"
               "    Stop the running server.""\n"
               "    Example: /stop""\n""\n"
               "/exit, /e""\n"
               "    Exit the program.""\n"
               "    Example: /exit""\n""\n"
               "/rcc""\n"
               "    Request the current number of connected clients from the server.""\n"
               "    Example: /rcc""\n""\n"
               "/add""\n"
               "    Add a server to operate on.""\n"
               "    Example: /add""\n""\n"
               "/remove""\n"
               "    Remove a server from operation.""\n"
               "    Example: /remove""\n""\n"
               "/i""\n"
               "    Set the current operating server by its integer ID.""\n"
               "    Example: /i 1""\n""\n"
               "/h""\n"
               "    Show this help menu.""\n"
               "    Example: /h""\n";
               break;
            }
            case PC_Exit: {
               NL_BREAK(nl, 0);
            }
            case PC_Add: {
               servers.emplace_back(SimpleChatServer(servers.size()));
               std::cout << "server added now: " << servers.size() << ", currentID: " << cSId << "\n";
               break;
            }
            case PC_Remove: {
               SimpleChatServer& scs = servers.back();
               scs.shutdown();
               servers.pop_back();
               if(cSId > servers.size()-1) cSId = servers.size()-1;
               std::cout << "server removed now: " << servers.size() << ", currentID: " << cSId << "\n";
               break;
            }
            case PC_ID: {
               auto id = StringUtil::parseArg<uint16_t>(args, 0);
               if(!id) {
                  std::cerr << "incorrect arg usage\n";
                  continue;
               }
               cSId = *id;
               if(cSId < 0) cSId = 0;
               if(cSId > servers.size()-1) cSId = servers.size()-1;
               std::cout << "server ided now: " << servers.size() << ", currentID: " << cSId << "\n";
               break;
            }
            case PC_StartServer: {
               auto port = StringUtil::parseArg<uint16_t>(args, 0);
               if(!port) {
                  std::cerr << "incorrect arg usage\n";
                  continue;
               }

               servers[cSId].start(*port);
               SimpleChatServer::printServer("Server Created..", servers[cSId].getPort(), true);
               break;
            }
            case PC_StopServer: {
               servers[cSId].close();
               SimpleChatServer::printServer("server closed");
               break;
            }
            case PC_ReqClientCount: {
               SimpleChatServer& scs = servers[cSId];
               auto lifeTGuard = scs.getHandle();
               asio::post(*scs.getContext(), [&, msg, lifeTGuard]() {
                  std::scoped_lock lock(lifeTGuard->guardMutex);
                  if(!lifeTGuard->parent) return;
                  std::cout << "client connections stored: " << lifeTGuard->parent->getConnections().size() << "\n";
               });
               std::cout << "server count: " << servers.size() << ", currentID: " << cSId << "\n";
               break;
            }
            default: {
               SimpleChatServer::printServer(""+msg);
               SimpleChatServer& scs = servers[cSId];
               auto lifeTGuard = scs.getHandle();
               asio::post(*scs.getContext(), [&, msg, lifeTGuard]() {
                  std::scoped_lock lock(lifeTGuard->guardMutex);
                  if(!lifeTGuard->parent) return;
                  for(auto& connection : lifeTGuard->parent->getConnections()) {
                     connection->send(StringUtil::stringToBytes(msg));
                  }
               });
               break;
            }
         }
         NL_CHECK(nl, 0);
      }
      
      for (size_t i = 0; i < servers.size(); i++) {
         servers[i].shutdown();
      }
   }

   std::cout << "skipped" << std::endl;
   return 0;
}