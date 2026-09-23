#include "SRC/Controller.h"

#include "SRC/Util/NestedLoops.h"
#include "SRC/Util/StringUtil.h"
#include <unordered_map>

enum Command {
   CMD_Exit,
   CMD_Message,
   CMD_Message_Client,
   CMD_Use,
   CMD_Remove,
   CMD_List_Instance,
   CMD_Id_Current,
   CMD_Resolve,
   CMD_StartServer,
   CMD_ConnectClient,
   CMD_Stop,
   CMD_Disconnect
};

std::unordered_map<std::string, Command> commands = {
   {"/exit", CMD_Exit},
   {"/use", CMD_Use},
   {"/u", CMD_Use},
   {"/remove", CMD_Remove},
   {"/rem", CMD_Remove},
   {"/id", CMD_Id_Current},
   {"/current", CMD_Id_Current},
   {"/ls", CMD_List_Instance},
   {"/l", CMD_List_Instance},
   {"/list", CMD_List_Instance},
   {"/resolve", CMD_Resolve},
   {"/res", CMD_Resolve},
   {"/server", CMD_StartServer},
   {"/s", CMD_StartServer},
   {"/connect", CMD_ConnectClient},
   {"/c", CMD_ConnectClient},
   {"/stop", CMD_Stop},
   {"/st", CMD_Stop},
   {"/close", CMD_Stop},
   {"/cl", CMD_Stop},
   {"/disconnect", CMD_Disconnect},
   {"/d", CMD_Disconnect},
   {"/m", CMD_Message_Client},
   {"/msg", CMD_Message_Client},
   {"/message", CMD_Message_Client},
};

int main() {
   std::string msg;
   std::vector<std::string> args;
   std::string errorMsg;
   Command cmd;
   int cSId = 0;

   std::cout << "SimpleChat CLI\n";

   SimpleChatController controller;

   SN::NestedLoop nl;
   while(true) {
      std::getline(std::cin, msg);
      args = StringUtil::split(msg, " ");
      if (args.empty()) continue;
      std::string cmdStr = args[0];
      shift_left(args.begin(), args.end(), 1);

      auto command = StringUtil::parseOptions(cmdStr, commands);
      
      cmd = CMD_Message;
      if (command) cmd = *command;
      

      switch (cmd) {
         case CMD_Exit:
            NL_BREAK(nl, 0);
         case CMD_Use: {
            auto id = StringUtil::parseArg<std::string>(args, 0);
            if(!id) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }
            controller.use(*id);
            break;
         }
         case CMD_Remove: {
            auto id = StringUtil::parseArg<std::string>(args, 0);
            if(!id) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }
            if(controller.remove(*id)) {
               std::cout << "removed instance: " << *id << " current id now " << controller.getCurrentSelectedId() << "\n";
            } else std::cout << "failed to remove instance\n";
            break;
         }
         case CMD_List_Instance : {
            std::cout << "Instances:\n";

            for(auto& [id, instance] : controller.getInstances()) {
               std::cout << "id: " << id << " -> ";

               if(auto client = std::dynamic_pointer_cast<SCTClient>(instance)) {
                  std::cout << "TCP Client\n";
               } else if(auto client = std::dynamic_pointer_cast<SCUClient>(instance)) {
                  std::cout << "UDP Client\n";
               } else if(auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
                  std::cout << "TCP Server\n";
                  std::cout << "   connections: " << server->getConnections().size() << '\n';
               } else if(auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
                  std::cout << "UDP Server\n";
                  std::cout << "   connections: " << server->getConnections().size() << '\n';
               } else {
                  std::cout << "Unknown Instance\n";
               }
            }
         }
         case CMD_Id_Current: {
            std::cout << "current id: " << controller.getCurrentSelectedId() << "\n";
            break;
         }
         case CMD_Resolve: {
            auto protocol = StringUtil::parseOptions(args, 0, std::unordered_map<std::string, bool> {
               {"tcp", true},
               {"udp", false}
            });

            int arg = protocol ? 1 : 0;
            bool tcp = protocol.value_or(true);

            auto ip = StringUtil::parseArg<std::string>(args, arg);
            auto port = StringUtil::parseArg<uint16_t>(args, arg + 1);

            if(!ip || !port) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }

            controller.resolve(*ip, *port, tcp);
            break;
         }
         case CMD_ConnectClient: {
            auto protocol = StringUtil::parseOptions(args, 0, std::unordered_map<std::string, bool> {
               {"tcp", true},
               {"udp", false}
            });

            int arg = protocol ? 1 : 0;
            bool tcp = protocol.value_or(true);

            auto ip = StringUtil::parseArg<std::string>(args, arg);
            auto port = StringUtil::parseArg<uint16_t>(args, arg + 1);

            if(!ip || !port) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }

            controller.connect(*ip, *port, tcp);
            break;
         }
         case CMD_StartServer: {
            auto protocol = StringUtil::parseOptions(args, 0, std::unordered_map<std::string, bool> {
               {"tcp", true},
               {"udp", false}
            });

            int arg = protocol ? 1 : 0;
            bool tcp = protocol.value_or(true);

            auto port = StringUtil::parseArg<uint16_t>(args, arg);
            if(!port) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }
            controller.start(*port, tcp);
            break;
         }
         case CMD_Stop: {
            controller.stop();
            break;
         }
         case CMD_Disconnect: {
            auto clientID = StringUtil::parseArg<int>(args, 0);
            if(clientID) {
               controller.disconnect(*clientID);
            } else controller.disconnect();
            break;
         }
         case CMD_Message_Client: {
            auto clientID = StringUtil::parseArg<int>(args, 0);
            msg = "";
            for (int i = 1; i < args.size()-1; i++) {
               if(i == args.size()-1) msg += " ";
               msg += args[i];
            }
            if(!clientID) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }
            std::cout << "msg:\"" << msg << "\"\n";
            controller.send(msg, *clientID);
            break;
         }
         case CMD_Message: {
            controller.send(msg);
            break;
         }
         default:
            break;
      }
      NL_CHECK(nl, 0);
   }

   std::cout << "exited" << std::endl;
   return 0;
};