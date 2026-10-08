#ifndef CLI_INTERPRETER_H
#define CLI_INTERPRETER_H

#include "Controller.h"

#include "Instance.h"
#include "Util/StringUtil.h"
#include <string>
#include <unordered_map>

class CliInterpreter {
public:
   CliInterpreter(std::shared_ptr<MessageCallback> messageCallback, std::shared_ptr<SimpleChatController> controller) : messageCallback(messageCallback), controller(controller) {}

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
      CMD_Disconnect,
      CMD_Connection_Count
   };

   void parseCommandMessage(std::string commandMsg, bool& breakWhile) {
      msg = commandMsg;

      args = StringUtil::split(msg, " ");
      if (args.empty()) return;
      std::string cmdStr = args[0];
      shift_left(args.begin(), args.end(), 1);

      auto command = StringUtil::parseOptions(cmdStr, commands);
      
      cmd = CMD_Message;
      if (command) cmd = *command;

      switch (cmd) {
         case CMD_Exit: {
            breakWhile = true;
            return;
         }
         case CMD_Use: {
            auto id = StringUtil::parseArg<std::string>(args, 0);
            if(!id) {
               if(messageCallback) (*messageCallback)("incorrect arg usage\n", MessageType::Warning);
               return;
            }
            controller->use(*id);
            break;
         }
         case CMD_Remove: {
            auto id = StringUtil::parseArg<std::string>(args, 0);
            if(!id) {
               if(messageCallback) (*messageCallback)("incorrect arg usage\n", MessageType::Warning);
               return;
            }
            if(controller->remove(*id)) {
               if(messageCallback) (*messageCallback)("removed instance: " + *id + " current id now " + controller->getCurrentSelectedId() + "\n", MessageType::Info);
            } else if(messageCallback) (*messageCallback)("failed to remove instance\n", MessageType::Warning);
            break;
         }
         case CMD_List_Instance : {
            if(messageCallback) (*messageCallback)("Instances:\n", MessageType::Info);

            for(auto& [id, instance] : controller->getInstances()) {
               if(messageCallback) (*messageCallback)("id: " + id + " -> ", MessageType::Info);

               if(auto client = std::dynamic_pointer_cast<SCTClient>(instance)) {
                  if(messageCallback) (*messageCallback)("TCP Client\n", MessageType::Info);
               } else if(auto client = std::dynamic_pointer_cast<SCUClient>(instance)) {
                  if(messageCallback) (*messageCallback)("UDP Client\n", MessageType::Info);
               } else if(auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
                  if(messageCallback) (*messageCallback)("TCP Server\n", MessageType::Info);
                  if(messageCallback) (*messageCallback)("   connections: " + std::to_string(server->getConnections().size()) + '\n', MessageType::Info);
               } else if(auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
                  if(messageCallback) (*messageCallback)("UDP Server\n", MessageType::Info);
                  if(messageCallback) (*messageCallback)("   connections: " + std::to_string(server->getConnections().size()) + '\n', MessageType::Info);
               } else {
                  if(messageCallback) (*messageCallback)("Unknown Instance\n", MessageType::Info);
               }
            }
         }
         case CMD_Id_Current: {
            if(messageCallback) (*messageCallback)("current id: " + controller->getCurrentSelectedId() + "\n", MessageType::Info);
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
               if(messageCallback) (*messageCallback)("incorrect arg usage\n", MessageType::Warning);
               return;
            }

            controller->resolve(*ip, *port, tcp);
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
               if(messageCallback) (*messageCallback)("incorrect arg usage\n", MessageType::Warning);
               return;
            }

            controller->connect(*ip, *port, tcp);
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
               if(messageCallback) (*messageCallback)("incorrect arg usage\n", MessageType::Warning);
               return;
            }
            controller->start(*port, tcp);
            break;
         }
         case CMD_Stop: {
            controller->stop();
            break;
         }
         case CMD_Disconnect: {
            auto clientID = StringUtil::parseArg<int>(args, 0);
            if(clientID) {
               if(messageCallback) (*messageCallback)("force disconnecting client with id: " + std::to_string(*clientID) + "\n", MessageType::Info);
               controller->disconnect(*clientID);
            } else {
               if(messageCallback) (*messageCallback)("disconnecting all clients\n", MessageType::Info);
               controller->disconnect();
            }
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
               if(messageCallback) (*messageCallback)("incorrect arg usage\n", MessageType::Warning);
               return;
            }
            if(messageCallback) (*messageCallback)("msg:\"" + msg + "\"\n", MessageType::Info);
            controller->send(msg, *clientID);
            break;
         }
         case CMD_Connection_Count: {
            controller->printServerConnectionCount();
            break;
         }
         case CMD_Message: {
            controller->send(msg);
            break;
         }
         default:
            break;
      }
      // TODO: NL_CHECK(nl, 0);
   }
public:
   std::shared_ptr<SimpleChatController> controller;
   std::string msg;
   std::vector<std::string> args;
   std::string errorMsg;
   Command cmd;
   std::shared_ptr<MessageCallback> messageCallback;

private:
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
      {"/cc", CMD_Connection_Count},
      {"/connectioncount", CMD_Connection_Count}
   };
};

#endif // CLI_INTERPRETER_H