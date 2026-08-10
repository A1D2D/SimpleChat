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

   std::cout << "SimpleChat: Peer\n";
   
   {
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
               if(!port) {
                  std::cerr << "incorrect arg usage\n";
                  continue;
               }

               //TODO: start server
               std::cout << "Server Created.." << /*port <<*/ std::endl;
               break;
            }
            case SC_StopServer: {
               //TODO: start server
               std::cout << "server closed" << std::endl;
               break;
            }
            case SC_ReqConnectionCount: {
               std::cout << "server connection count: " << /*connectiion count <<*/ std::endl;
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