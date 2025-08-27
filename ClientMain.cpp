#include <VORTEX_MP/NestedLoops>
#include <iostream>
#include <string>

#include "SRC/Util/StringUtil.h"

#include "SRC/Networking/PacketNet.h"

enum ClientCommand {
   CC_Connect,
   CC_Disconnect,
   CC_Exit,
   CC_Message,
   CC_Test_F
};

class SimpleChatClient : public PN::PacketNetClient<> {
protected:
   void onPacket(const PN::DefaultPacket& data) override {
      std::cout << "[Server]: " << StringUtil::bytesToString(data.data) << std::endl;
   }

   void onConnect() override {
      sendHandshake();
   }
};
/*
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
      {"/exit", CC_Exit}
   };

   Colorb::SKY_BLUE.printAnsiStyle();
   std::cout << "SimpleChat\n";
   resetAnsiStyle();

   NestedLoop nl;

   SimpleChatClient client;

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
         case CC_Exit: {
            NL_BREAK(nl, 0);
         }
         case CC_Connect: {
            auto ip = StringUtil::parseArg<std::string>(args, 0);
            auto port = StringUtil::parseArg<ushort_16>(args, 1);
            if(!port || !ip) {
               std::cerr << "incorrect arg usage\n";
               continue;
            }

            SN::StreamedNetClient::printClient("Connecting to Server..", *ip, *port, true);
            client.autoConnect(*ip, *port);
            break;
         }
         case CC_Disconnect: {
            client.disconnect();
            break;
         }
         default: {
            SN::StreamedNetClient::printClient(""+msg);
            client.sendPacket(StringUtil::stringToBytes(msg));
            break;
         }
      }
      NL_CHECK(nl,0);
   }
   std::cout << "skipped" << std::endl;
   client.joinThread();
   
   return 0;
}
*/

struct Alma : PN::Serializable {
   std::string v;

   SERIALIZABLE(v)
};

struct Kusolj : PN::Serializable {
   int a;
   float b;
   unsigned long c;
   std::string val;
   std::vector<int> lol;
   std::vector<Alma> d;

   SERIALIZABLE(a, b, c, val, lol, d)
};

std::array<unsigned char,2> getHex(unsigned char byte) {
   const unsigned char HexChars[6] = {'A','B','C','D','E','F'};
   std::array<unsigned char, 2> array{};
   unsigned char bits = byte & 0x0F;
   for (int i = 0; i < 2; ++i) {
      if(i == 0) {
         bits = (byte & 0xF0) >> 4;
      }
      if(bits < 10) {
         array[i] = '0' + bits;
      } else array[i] = HexChars[bits-10];
   }
   return array;
}

std::string getHex(const std::vector<unsigned char>& buffer) {
   std::string hexString(buffer.size()*3-1,' ');
   for (int i = 0; i < buffer.size(); ++i) {
      auto hexChars = getHex(buffer[i]);
      hexString[i*3] = (char)hexChars[0];
      hexString[i*3+1] = (char)hexChars[1];
   }
   return hexString;
}

int main(int argc, const char** argv) {
   Kusolj k;
   Kusolj z;
   std::vector<ubyte_8> buf;

   k.a = 420;
   k.b = 3.14;
   k.c = 50;
   k.val = "venator";
   k.lol = {1, 2, 3, 4, 5, 6};
   Alma a;
   a.v = "helna";
   k.d = { a };

   buf = k.serialize();
   ulong_64 id = 0;
   z.deserialize(buf, id);

   std::cout << "str->" << StringUtil::bytesToString(k.serialize()) << "\n";
   std::cout << "hex->" << getHex(k.serialize()) << "\n";

   std::cout << z.a << " : " << z.b << " : " << z.c << " : " << z.val << " : " << z.lol[0] << " : " << z.lol[1] << " : " << z.lol[2] << " : " << z.lol[3] << " : " << z.lol[4] << " : " << z.lol[5] << k.d[0].v << "\n";

   return 0;
}