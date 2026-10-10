#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <webview/webview.h>

#include "SRC/CLIInterpreter.h"
#include "SRC/Controller.h"


#ifdef _WIN32
   #define AppMain WINAPI WinMain(HINSTANCE /*hInst*/, HINSTANCE /*hPrevInst*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
#else
   #define AppMain main()
#endif

struct UIInstance {
   std::string id;
   std::vector<std::string> connectedClients;
};

struct UIInstType {
   std::string typeName;
   std::vector<UIInstance> instances;
};

int AppMain {
   try {
      webview::webview w(true, nullptr);

      w.set_title("Simple Chat");
      w.set_size(1280, 720, WEBVIEW_HINT_NONE);

      w.init(R"(
         window.onerror = (message, source, line, column, error) => {
            logerr("MESSAGE: " + message);
         };
      )");

      auto callback = std::make_shared<MessageCallback>([&w](std::string message, MessageType type) {
         switch (type) {
         case MessageType::Info:
            std::cout << message;

            w.dispatch([&w, message = std::move(message)]() { w.eval("consoleInfo(" + nlohmann::json(message).dump() + ");"); });
            break;

         case MessageType::Warning:
            std::cerr << message;

            w.dispatch([&w, message = std::move(message)]() { w.eval("consoleError(" + nlohmann::json(message).dump() + ");"); });
            break;

         default:
            std::cerr << "invalid type\n";
            break;
         }
      });

      auto controller = std::make_shared<SimpleChatController>(callback);
      CliInterpreter interpreter(callback, controller);

      // C++ binds
      w.bind("log", [](std::string message) {
         std::cout << "[JS] " << message << std::endl;
         return "";
      });

      w.bind("logerr", [](std::string message) {
         std::cout << "[jserr] " << message << std::endl;
         return "";
      });

      w.bind("use", [controller](std::string instanceID) {
         auto json = nlohmann::json::parse(instanceID);
         controller->use(json[0].get<std::string>());
         return "";
      });

      w.bind("stopInstance", [controller](std::string str) {
         controller->stop();
         return "";
      });

      w.bind("disconnect", [controller](std::string info) {
         auto json = nlohmann::json::parse(info);
         int clientID = json[0].get<int>();
         if (clientID == -1) {
            controller->disconnect(clientID);
         } else
            controller->disconnect();
         return "";
      });

      w.bind("remove", [controller](std::string instanceID) {
         auto json = nlohmann::json::parse(instanceID);
         controller->remove(json[0].get<std::string>());
         return "";
      });

      w.bind("resolve", [controller](std::string command) {
         auto json = nlohmann::json::parse(command);
         std::string protocol = json[0].get<std::string>();
         std::string ip = json[1].get<std::string>();
         int port = json[2].get<int>();
         controller->resolve(ip, port, protocol == "tcp");
         return "";
      });

      w.bind("connect", [controller](std::string command) {
         auto json = nlohmann::json::parse(command);
         std::string protocol = json[0].get<std::string>();
         std::string ip = json[1].get<std::string>();
         int port = json[2].get<int>();
         controller->connect(ip, port, protocol == "tcp");
         return "";
      });

      w.bind("start", [controller](std::string command) {
         auto json = nlohmann::json::parse(command);
         std::string protocol = json[0].get<std::string>();
         int port = json[1].get<int>();
         controller->start(port, protocol == "tcp");
         return "";
      });

      w.bind("getInstances", [controller](std::string throwAway) {
         auto instances = controller->getInstances();
         std::vector<UIInstType> typeInsts;

         for (auto& [id, instance] : instances) {
            std::string type = "Undefined";
            std::vector<std::string> connectedClients;
            if (auto client = std::dynamic_pointer_cast<SCTClient>(instance)) {
               type = "TCP Client";
            } else if (auto client = std::dynamic_pointer_cast<SCUClient>(instance)) {
               type = "UDP Client";
            } else if (auto server = std::dynamic_pointer_cast<SCTServer>(instance)) {
               type = "TCP Server";
               for (auto& connection : server->getConnections()) {
                  tcp::endpoint endpoint = connection->getEndpoint();
                  connectedClients.push_back("[" + endpoint.address().to_string() + "]:" + std::to_string(endpoint.port()));
               }
            } else if (auto server = std::dynamic_pointer_cast<SCUServer>(instance)) {
               type = "UDP Server";
               for (auto& connection : server->getConnections()) {
                  udp::endpoint endpoint = connection->getEndpoint();
                  connectedClients.push_back("[" + endpoint.address().to_string() + "]:" + std::to_string(endpoint.port()));
               }
            }

            auto it = std::find_if(typeInsts.begin(), typeInsts.end(), [type](const UIInstType& typeInst) { return typeInst.typeName == type; });

            UIInstance uiInstance(id, connectedClients);

            if (it != typeInsts.end()) {
               it->instances.push_back(uiInstance);
            } else {
               typeInsts.push_back(UIInstType{type, {uiInstance}});
            }
         }

         nlohmann::json result = nlohmann::json::array();

         for (const auto& type : typeInsts) {
            nlohmann::json category;
            category["type"] = type.typeName;
            category["instances"] = nlohmann::json::array();

            for (const auto& instance : type.instances) {
               nlohmann::json jInst;
               jInst["id"] = instance.id;
               jInst["clients"] = nlohmann::json::array();
               for (const auto& client : instance.connectedClients) {
                  nlohmann::json serverConnection;
                  serverConnection["id"] = client;
                  jInst["clients"].push_back(serverConnection);
               }
               category["instances"].push_back(jInst);
            }

            result.push_back(category);
         }
         return result.dump();
      });

      w.bind("getMessages", [controller](std::string throwAway){
         auto messages = controller->getMessages();

         nlohmann::json result = nlohmann::json::array();

         for(const auto& message : messages) {
            nlohmann::json jMessage;
            jMessage["msg"] = message.message;
            jMessage["type"] = message.type;
            jMessage["id"] = message.id;
            result.push_back(jMessage);
         }

         return result.dump();
      });

      w.bind("sendMessage", [controller](std::string info) {
         auto json = nlohmann::json::parse(info);
         int clientID = json[0].get<int>();
         auto message = json[1].get<std::string>();
         if (clientID == -1) {
            controller->send(message);
         } else {
            controller->send(message, clientID);
         }
         std::cout << "message: " << message << "\n";
         return "";
      });

      w.bind("sendCommand", [controller, interpreter](std::string commandJson) mutable {
         auto json = nlohmann::json::parse(commandJson);
         auto command = json[0].get<std::string>();
         bool breakWhile;
         interpreter.parseCommandMessage(command, breakWhile);
         std::cout << "command: " << command << "\n";
         return "";
      });

      w.navigate("file:///" + std::filesystem::absolute("index.html").generic_string());

      w.run();
   } catch (const webview::exception& e) {
      std::cerr << e.what() << '\n';
      return 1;
   }
   return 0;
}