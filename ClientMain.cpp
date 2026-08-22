#include "SRC/Util/NestedLoops.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "SRC/Networking/StreamedNet.h"
#include "SRC/Util/StringUtil.h"
#include <unordered_map>
#include <thread>

class MinimalResolver;

MinimalResolver* res;

enum ClientCommand {
   CC_Connect,
   CC_Disconnect,
   CC_Exit,
   CC_Message,
   SC_Add,
   SC_Remove,
   SC_ID,
   SC_Help
};

class FullResolver : public SN::Resolver {
public:
   FullResolver() : SN::Resolver(), running(std::make_shared<std::atomic_bool>(true)) {
      setContext(context);

      th = std::make_unique<std::thread>([ctx = context, run = running]() mutable {
         while (run->load() && ctx.usage()) {
            ctx.poll();
         }
      });
   }

   FullResolver(FullResolver&& other) noexcept : SN::Resolver(std::move(other)), context(std::move(other.context)), th(std::move(other.th)), mutex(std::move(other.mutex)), running(std::move(other.running)) {}

   FullResolver& operator=(FullResolver&& other) noexcept {
      if (this == &other) return *this;

      running->store(false);
      if (th && th->joinable()) th->join();

      SN::Resolver::operator=(std::move(other));
      context = std::move(other.context);
      th = std::move(other.th);
      running = std::move(running);

      return *this;
   }

   ~FullResolver() {
      running->store(false);
      if (th && th->joinable()) th->join();
   }

   void onTcpResolve(tcp::resolver::results_type resultEndpoints) override {
      std::cout << "Resolved endpoints: \n";
      for (auto it = resultEndpoints.begin(); it != resultEndpoints.end(); ++it) {
         tcp::endpoint endpoint = it->endpoint();
         std::cout << "tcp: " << endpoint.address().to_string() << ":" << endpoint.port() << '\n';
      }
   }

   void onUdpResolve(udp::resolver::results_type resultEndpoints) override {
      std::cout << "Resolved endpoints: \n";
      for (auto it = resultEndpoints.begin(); it != resultEndpoints.end(); ++it) {
         udp::endpoint endpoint = it->endpoint();
         std::cout << "udp: " << endpoint.address().to_string() << ":" << endpoint.port() << '\n';
      }
   }

private:
   SN::Context context;
   std::unique_ptr<std::thread> th;
   std::unique_ptr<std::mutex> mutex;
   std::shared_ptr<std::atomic_bool> running;
};

class MinimalResolver : public SN::Resolver {
public:
   int counter = 0;

   MinimalResolver(SN::Context context) : SN::Resolver(context) {}

   void onTcpResolve(tcp::resolver::results_type resultEndpoints) override {
      std::cout << "Resolved endpoints: \n";
      for (auto it = resultEndpoints.begin(); it != resultEndpoints.end(); ++it) {
         tcp::endpoint endpoint = it->endpoint();
         std::cout << "tcp: " << endpoint.address().to_string() << ":" << endpoint.port() << '\n';
      }
      delete res;
      res = nullptr;
   }

   void onUdpResolve(udp::resolver::results_type resultEndpoints) override {
      std::cout << "Resolved endpoints: \n";
      for (auto it = resultEndpoints.begin(); it != resultEndpoints.end(); ++it) {
         udp::endpoint endpoint = it->endpoint();
         std::cout << "udp: " << endpoint.address().to_string() << ":" << endpoint.port() << '\n';
      }
   }

   void onTick() override {
      counter++;
      if(counter % 5000000 == 0) {
         std::cout << ".";
      }
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
      {"/exit", CC_Exit},
      {"/add", SC_Add},
      {"/remove", SC_Remove},
      {"/i", SC_ID},
      {"/h", SC_Help}
   };

   std::cout << "SimpleChat: Client\n";

   {
      SN::Context context;

      res = new MinimalResolver(context);

      std::thread testThread = std::thread([context]() mutable {
         while (context.usage()) {
            context.poll();
         }
      });

      SN::NestedLoop nl;
      for (;;) {
         std::getline(std::cin, msg);
         args = StringUtil::split(msg, " ");
         if (args.empty()) continue;
         std::string cmdStr = args[0];
         std::shift_left(args.begin(), args.end(), 1);

         auto it = commands.find(cmdStr);
         if (it == commands.end()) {
            if (msg.size() >= 0 && msg[0] == '/') {
               std::cout << "Unknown command\n";
               continue;
            }
            cmd = CC_Message;
         } else {
            cmd = it->second;
         }
         switch (cmd) {
            case SC_Help: {
               std::cout <<
                  "Available Commands:""\n""\n"
                  "/connect, /c""\n"
                  "    Connect the client. argument: [ip] [port] to specify the server ip and port.""\n"
                  "    Example: /c localhost 8080""\n""\n"
                  "/stop, /d""\n"
                  "    Stop the running client.""\n"
                  "    Example: /stop""\n""\n"
                  "/exit, /e""\n"
                  "    Exit the program.""\n"
                  "    Example: /exit""\n""\n"
                  "/add""\n"
                  "    Add a client to operate on.""\n"
                  "    Example: /add""\n""\n"
                  "/remove""\n"
                  "    Remove a client from operation.""\n"
                  "    Example: /remove""\n""\n"
                  "/i""\n"
                  "    Set the current operating client by its integer ID.""\n"
                  "    Example: /i 1""\n""\n"
                  "/h""\n"
                  "    Show this help menu.""\n"
                  "    Example: /h""\n";
               break;
            }
            case CC_Exit: {
               NL_BREAK(nl, 0);
            }
            case SC_Add: {
               //TODO: for dev removed
               res->doTick();
               break;
            }
            case SC_Remove: {
               //TODO: for dev removed
               res->stopTick();
               break;
            }
            case SC_ID: {
               //TODO: for dev removed
               break;
            }
            case CC_Connect: {
               auto ip = StringUtil::parseArg<std::string>(args, 0);
               auto port = StringUtil::parseArg<uint16_t>(args, 1);
               if (!port || !ip) {
                  std::cerr << "incorrect arg usage\n";
                  continue;
               }

               std::cout << "Connecting to Server..\n" << "ip:" << *ip << " port:" << *port << std::endl;
               res->resolve<SN::NetworkMode::TCP>(*ip, *port);
               res->resolve<SN::NetworkMode::UDP>(*ip, *port);
               //TODO: connect
               break;
            }
            case CC_Disconnect: {
               delete res;
               //TODO: disconnect
               break;
            }
            default: {
               std::cout << "sending message: " << msg << std::endl;
               //TODO: send
               break;
            }
         }
         NL_CHECK(nl, 0);
      }
   }
   std::cout << "skipped" << std::endl;
   return 0;
}