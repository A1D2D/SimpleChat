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
            case CC_Connect: {
               auto ip = StringUtil::parseArg<std::string>(args, 0);
               auto port = StringUtil::parseArg<uint16_t>(args, 1);
               if(!port || !ip) {
                  std::cerr << "incorrect arg usage\n";
                  continue;
               }

               std::cout << "Connecting to Server.." << " ip:" << *ip << " port:" << *port << std::endl;
               //TODO: connect
               break;
            }
            case CC_Disconnect: {
               //TODO: disconnect
               break;
            }
            default: {
               std::cout << "sending message: " << msg << std::endl;
               //TODO: send
               break;
            }
         }
         NL_CHECK(nl,0);
      }
   }
   std::cout << "skipped" << std::endl;
   return 0;
}