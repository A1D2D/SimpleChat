#include "SRC/Util/NestedLoops.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "SRC/Networking/StreamedNet.h"
#include "SRC/Util/StringUtil.h"
#include <unordered_map>
#include <thread>

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

class SimpleChatClient : public SN::Client {
public:
   SimpleChatClient(int clientID_) : SN::Client::Client(), clientID(clientID_) {}

   // using SN::Client::Client;
   int clientID = 0;
   std::shared_ptr<std::thread> thread;
   std::shared_ptr<std::mutex> mutex;
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
      std::cout << "cID: " << clientID << ", Server: ";
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
   int cCId = 0;

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

   // Colorb::SKY_BLUE.printAnsiStyle();
   std::cout << "SimpleChat: Client\n";
   // resetAnsiStyle();

   {
      std::vector<SimpleChatClient> clients;
      {
         SimpleChatClient client(clients.size());
         client.mutex = std::make_shared<std::mutex>();
         client.thread = std::make_shared<std::thread>([](asio::io_context* context, std::shared_ptr<std::mutex> mutex){
            int handlers = 1;
            while (handlers) {
               // mutex->lock();
               // std::cout << "run start\n";
               context->poll_one();
               // std::cout << "run end\n";
               // handlers = context->run();
               // mutex->unlock();
            }
         }, client.getContext()->get().ptr(), client.mutex);

         clients.emplace_back(std::move(client));
      }

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
         if(clients.size() <= 0 && (cmd != SC_Add && cmd != SC_Help)) {
            std::cout << "client must be added before using any other command\n";
            continue;
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
               SimpleChatClient client(clients.size());
               client.mutex = std::make_shared<std::mutex>();
               client.thread = std::make_shared<std::thread>([](asio::io_context* context, std::shared_ptr<std::mutex> mutex){
                  int handlers = 1;
                  while (handlers > 0) {
                     // std::lock_guard<std::mutex> guard(*mutex);
                     handlers = context->poll();
                  }
               }, client.getContext()->get().ptr(), client.mutex);

               clients.emplace_back(std::move(client));
               std::cout << "client added now: " << clients.size() << ", currentID: " << cCId << "\n";
               break;
            }
            case SC_Remove: {
               SimpleChatClient& scc = clients.back();
               std::shared_ptr<std::thread> th = scc.thread;
               clients.pop_back();
               if(th->joinable()) th->join();
               if(cCId > clients.size()-1) cCId = clients.size()-1;
               std::cout << "client removed now: " << clients.size() << ", currentID: " << cCId << "\n";
               break;
            }
            case SC_ID: {
               auto id = StringUtil::parseArg<uint16_t>(args, 0);
               if(!id) {
                  std::cerr << "incorrect arg usage\n";
                  continue;
               }
               cCId = *id;
               if(cCId < 0) cCId = 0;
               if(cCId > clients.size()-1) cCId = clients.size()-1;
               std::cout << "client ided now: " << clients.size() << ", currentID: " << cCId << "\n";
               break;
            }
            case CC_Connect: {
               auto ip = StringUtil::parseArg<std::string>(args, 0);
               auto port = StringUtil::parseArg<uint16_t>(args, 1);
               if(!port || !ip) {
                  std::cerr << "incorrect arg usage\n";
                  continue;
               }

               SN::Client::printClient("Connecting to Server..", *ip, *port, true);
               //TODO: fix lock std::lock_guard<std::mutex> guard(*clients[cCId].mutex);
               clients[cCId].mutex->lock();
               clients[cCId].resolve(*ip, *port);
               clients[cCId].mutex->unlock();
               break;
            }
            case CC_Disconnect: {
               clients[cCId].disconnect();
               break;
            }
            default: {
               SN::Client::printClient(""+msg);
               clients[cCId].send(StringUtil::stringToBytes(msg));
               break;
            }
         }
         NL_CHECK(nl,0);
      }
      std::vector<std::shared_ptr<std::thread>> threads;

      for (auto& client : clients) {
         if (client.thread) threads.push_back(std::move(client.thread));
      }
      clients.clear();

      for (auto& thread : threads) {
         if (thread && thread->joinable()) thread->join();
      }
      threads.clear();
   }
   std::cout << "skipped" << std::endl;
   return 0;
}