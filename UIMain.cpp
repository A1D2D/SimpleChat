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

class OutputFormatter {
private:
   std::string output;
   bool insideBrackets = false;
   bool addSpace = false;

   void add(const std::string& text, bool brackets) {
      if (brackets) {
         if (!insideBrackets) {
            output += addSpace ? " <" : "<";
            insideBrackets = true;
         } else {
            output += ' ';
         }
         output += text;
      } else {
         if (insideBrackets) {
            output += addSpace ? "> " : ">";
            insideBrackets = false;
         }
         output += text;
      }
   }

   void add(unsigned char byte, bool brackets) {
      if (brackets) {
         // Using a standard snprintf for hex just in case MRW::getHex has issues
         char hex[3];
         snprintf(hex, sizeof(hex), "%02X", byte);
         std::string str(hex);
         add(str, true);
      } else {
         add(std::string(1, static_cast<char>(byte)), false);
      }
   }

   std::string finish() {
      if (insideBrackets) {
         output += ">";
         insideBrackets = false;
      }
      return output;
   }

public:
   static std::string sanitizeUTF8(const std::string& input, bool asciiOnly = false) {
      OutputFormatter formatter;
      formatter.output.reserve(input.size());

      size_t i = 0;

      while (i < input.size()) {
         const auto c = static_cast<unsigned char>(input[i]);

         // 1. ASCII Check (Printable + Tab/LF/CR)
         if ((c >= 0x20 && c <= 0x7E) || c == 0x09 || c == 0x0A || c == 0x0D) {
            formatter.add(std::string(1, static_cast<char>(c)), false);
            ++i;
            continue;
         }

         // 2. UTF-8 Check
         if (!asciiOnly) {
            size_t length = 0;

            if (c >= 0xC2 && c <= 0xDF)
               length = 2;
            else if (c >= 0xE0 && c <= 0xEF)
               length = 3;
            else if (c >= 0xF0 && c <= 0xF4)
               length = 4;

            bool valid = length != 0 && (input.size() - i) >= length;
            uint32_t codepoint = 0;

            if (valid) {
               if (length == 2)
                  codepoint = c & 0x1F;
               else if (length == 3)
                  codepoint = c & 0x0F;
               else
                  codepoint = c & 0x07;

               for (size_t j = 1; j < length; ++j) {
                  const auto b = static_cast<unsigned char>(input[i + j]);
                  if ((b & 0xC0) != 0x80) {
                     valid = false;
                     break;
                  }
                  codepoint = (codepoint << 6) | (b & 0x3F);
               }
            }

            if (valid) {
               const auto second = static_cast<unsigned char>(input[i + 1]);

               // Reject overlong encodings, surrogates and invalid code points.
               if ((length == 2 && codepoint < 0x80) || (length == 3 && codepoint < 0x800) || (length == 4 && codepoint < 0x10000) ||
                   (c == 0xE0 && second < 0xA0) || (c == 0xED && second >= 0xA0) || (c == 0xF0 && second < 0x90) || (c == 0xF4 && second > 0x8F) ||
                   (codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) {
                  valid = false;
               }

               // --- NEW: Reject unprintable/unknown characters that render as '' ---
               if (codepoint == 0xFFFD ||                                                 // The literal '' replacement character
                   codepoint == 0xFFFE ||                                                 // Non-characters
                   codepoint == 0xFFFF || (codepoint >= 0x0080 && codepoint <= 0x009F)) { // C1 Control Characters
                  valid = false;
               }
            }

            if (valid) {
               formatter.add(input.substr(i, length), false);
               i += length;
               continue;
            }
         }

         // 3. Fallback: Convert to Hex
         formatter.add(c, true);
         ++i;
      }

      return formatter.finish();
   }
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
         std::string utfString = OutputFormatter::sanitizeUTF8(message, false);
         std::string asciString = OutputFormatter::sanitizeUTF8(message, true);
         switch (type) {
         case MessageType::Info:
            std::cout << asciString;

            w.dispatch([&w, utfString = std::move(utfString)]() { w.eval("consoleInfo(" + nlohmann::json(utfString).dump() + ");"); });
            break;

         case MessageType::Warning:
            std::cerr << asciString;

            w.dispatch([&w, utfString = std::move(utfString)]() { w.eval("consoleError(" + nlohmann::json(utfString).dump() + ");"); });
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

      w.bind("getMessages", [controller](std::string throwAway) {
         auto messages = controller->getMessages();

         nlohmann::json result = nlohmann::json::array();

         for (const auto& message : messages) {
            nlohmann::json jMessage;
            jMessage["msg"] = OutputFormatter::sanitizeUTF8(message.message, false);
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
         std::cout << "message: " << OutputFormatter::sanitizeUTF8(message, true) << "\n";
         return "";
      });

      w.bind("sendCommand", [controller, interpreter](std::string commandJson) mutable {
         auto json = nlohmann::json::parse(commandJson);
         auto command = json[0].get<std::string>();
         bool breakWhile;
         interpreter.parseCommandMessage(command, breakWhile);
         std::cout << "command: " << OutputFormatter::sanitizeUTF8(command, true) << "\n";
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