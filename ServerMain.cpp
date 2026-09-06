#include <iostream>
#include <thread>

#include "SRC/Util/NestedLoops.h"

#include "SRC/Util/StringUtil.h"
#include "SRC/Networking/StreamedNet.h"

enum ServerCommand {
   SC_StartServer,
   SC_StopServer,
   SC_Exit,
   SC_Message,
   SC_ReqConnectionCount,
   SC_Add,
   SC_Remove,
   SC_ID,
   SC_Help
};

class CustomConnection : public SN::Connection<SN::NetworkMode::TCP> {
public:
   CustomConnection(SN::Server<SN::NetworkMode::TCP>* server, tcp::socket socket) : SN::Connection<SN::NetworkMode::TCP>(server, std::move(socket)) {}

   void onRead(std::vector<uint8_t> msg) override {
      std::string text(msg.begin(), msg.end());
      std::cout << text << "\n";
   }
};

class CustomServer : public SN::Server<SN::NetworkMode::TCP> {
public:
   CustomServer(SN::Context context) : SN::Server<SN::NetworkMode::TCP>(context) {}

   void onStart() override {
      std::cout << "start accept\n";
      startRead();
   }

   std::shared_ptr<SN::Connection<SN::NetworkMode::TCP>> onAccept(tcp::socket socket) override {
      std::error_code ec;

      std::cout << "=== Socket ===\n";
      std::cout << "Open: " << socket.is_open() << '\n';
      std::cout << "Native handle: " << socket.native_handle() << '\n';

      auto local = socket.local_endpoint(ec);
      if (!ec) std::cout << "Local: " << local.address().to_string() << ':' << local.port() << '\n';
      ec.clear();

      auto remote = socket.remote_endpoint(ec);
      if (!ec) std::cout << "Remote: " << remote.address().to_string() << ':' << remote.port() << '\n';
      
      std::cout << "Protocol: " << (socket.local_endpoint().protocol() == tcp::v4() ? "TCP/IPv4" : "TCP/IPv6") << '\n';
      return std::make_shared<CustomConnection>(this, std::move(socket));
   }
};

class CustomUDPConnection : public SN::Connection<SN::NetworkMode::UDP> {
public:
   CustomUDPConnection(SN::Server<SN::NetworkMode::UDP>* server, SN::Server<SN::NetworkMode::UDP>::UdpHandle handle) : SN::Connection<SN::NetworkMode::UDP>(server, handle) {}

   void onRead(std::vector<uint8_t> msg) override {
      std::string text(msg.begin(), msg.end());
      std::cout << text << "\n";
   }
};

class CustomUDPServer : public SN::Server<SN::NetworkMode::UDP> {
public:
   CustomUDPServer(SN::Context context) : SN::Server<SN::NetworkMode::UDP>(context) {}

   std::shared_ptr<SN::Connection<SN::NetworkMode::UDP>> onAccept(SN::Server<SN::NetworkMode::UDP>::UdpHandle handle, std::vector<uint8_t> msg) override {
      std::string text(msg.begin(), msg.end());
      if(text == "hellNah") return nullptr;
      std::cout << text << "\n";
      std::cout << "connect msg: " << text << "\n";
      return std::make_shared<CustomUDPConnection>(this, std::move(handle));
   }

   void onStart() override {
      std::cout << "start accept\n";
      startRead();
   }
};


int main() {
   std::string msg;
   std::vector<std::string> args;
   std::string errorMsg;
   ServerCommand cmd;
   int cSId = 0;


   std::unordered_map<std::string, ServerCommand> commands = {
      {"/server", SC_StartServer},
      {"/s", SC_StartServer},
      {"/stop", SC_StopServer},
      {"/d", SC_StopServer},
      {"/e", SC_Exit},
      {"/exit", SC_Exit},
      {"/rcc", SC_ReqConnectionCount},
      {"/add", SC_Add},
      {"/remove", SC_Remove},
      {"/i", SC_ID},
      {"/h", SC_Help}
   };

   std::cout << "SimpleChat: Server\n";

   {
      SN::Context context;
      std::shared_ptr<CustomUDPServer> server = std::make_shared<CustomUDPServer>(context);

      std::thread th = std::thread([&](){
         while(context.usage()) {
            context.poll();
         }
      });


      SN::NestedLoop nl;
      for (;;) {
         std::getline(std::cin, msg);
         args = StringUtil::split(msg, " ");
         if (args.empty()) continue;
         std::string cmdStr = args[0];
         shift_left(args.begin(), args.end(), 1);

         auto it = commands.find(cmdStr);
         if (it == commands.end()) {
            // std::cout << "Unknown command\n";
            // continue;
            cmd = SC_Message;
         } else {
            cmd = it->second;
         }


         switch (cmd) {
            case SC_Help: {
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
            case SC_Exit: {
               NL_BREAK(nl, 0);
            }
            case SC_Add: {
               //TODO: for dev removed
               break;
            }
            case SC_Remove: {
               //TODO: for dev removed
               break;
            }
            case SC_ID: {
               //TODO: for dev removed
               break;
            }
            case SC_StartServer: {
               auto port = StringUtil::parseArg<uint16_t>(args, 0);
               if (!port) {
                  std::cerr << "incorrect arg usage\n";
                  continue;
               }
               server->start(udp::endpoint(udp::v4(), *port));
               std::cout << "Server Created.." << /*port <<*/ std::endl;
               break;
            }
            case SC_StopServer: {
               server->disconnect();
               std::cout << "server closed" << std::endl;
               break;
            }
            case SC_ReqConnectionCount: {
               std::cout << "server connection count: " << server->getConnections().size() << std::endl;
               break;
            }
            default: {
               std::cout << "sending message to all: " << msg << std::endl;
               std::vector<uint8_t> data(msg.begin(), msg.end());
               for(auto& c : server->getConnections()) {
                  c->send(data);
               }
               break;
            }
         }
         NL_CHECK(nl, 0);
      }
   }

   std::cout << "skipped" << std::endl;
   return 0;
}