#ifndef NETWORK_PACKET_NET_TEMPLATED_H
#define NETWORK_PACKET_NET_TEMPLATED_H

#include "StreamedNet.h"
#include <type_traits>
#include <vector>
#include <cstring>

namespace PN {
   using namespace SN;
   using ulong_64 = std::uint64_t;

   struct PacketNetPacket {
      virtual ~PacketNetPacket() = default;
      virtual bool deserializeHead(std::vector<ubyte_8>& incoming) = 0;
      virtual bool checkHead() = 0;
      
      virtual bool deserializeEnd(std::vector<ubyte_8>& incoming) = 0;
      virtual bool checkEnd() = 0;

      virtual std::vector<ubyte_8>& readData(std::vector<ubyte_8>& incoming) = 0;
      virtual void erase(std::vector<ubyte_8>& incoming) = 0;

      virtual std::vector<ubyte_8> serialize() const = 0;
   };

   struct DefaultPacket : PacketNetPacket {
      constexpr static const std::string_view headSpecifier = "PN_PACKET";
      constexpr static const std::string_view endSpecifier = "<~PN>";
      std::string_view readHeadSpecifier{};
      std::string_view readEndSpecifier{};
      ulong_64 len;
      std::vector<ubyte_8> data;

      DefaultPacket(const std::vector<ubyte_8>& packetData) {
         data = packetData;
      }

      ~DefaultPacket() override {}

      bool deserializeHead(std::vector<ubyte_8>& incoming) override {
         if(incoming.size() < headSpecifier.size() + 8) return false;

         readHeadSpecifier = std::string_view(reinterpret_cast<char*>(incoming.data()), headSpecifier.size());
         std::memcpy(&len, incoming.data()+headSpecifier.size(), 8);

         return true;
      }

      bool checkHead() override {
         return readHeadSpecifier == headSpecifier;
      }

      bool deserializeEnd(std::vector<ubyte_8>& incoming) override {
         if(incoming.size() < headSpecifier.size() + 8 + len) return false;

         readEndSpecifier = std::string_view(reinterpret_cast<char*>(incoming.data() + headSpecifier.size() + 8 + len), endSpecifier.size());
         return true;
      }

      bool checkEnd() override {
         return readEndSpecifier == endSpecifier;
      }

      std::vector<ubyte_8>& readData(std::vector<ubyte_8>& incoming) override {
         data = std::vector<ubyte_8>(incoming.begin() + headSpecifier.size() + 8, incoming.begin() + headSpecifier.size() + 8 + len);
         return data;
      }

      void erase(std::vector<ubyte_8>& incoming) override {
         incoming.erase(incoming.begin(), incoming.begin() + headSpecifier.size() + 8 + len);
      }

      std::vector<ubyte_8> serialize() const override {
         std::vector<ubyte_8> buf(headSpecifier.size() + 8 + data.size() + endSpecifier.size());

         std::memcpy(buf.data(), headSpecifier.data(), headSpecifier.size());
         std::memcpy(buf.data() + headSpecifier.size(), &len, 8);
         std::memcpy(buf.data() + headSpecifier.size() + 8, data.data(), data.size());
         std::memcpy(buf.data() + headSpecifier.size() + 8 + data.size(), endSpecifier.data(), endSpecifier.size());

         return buf;
      }
   };

   template <typename T = DefaultPacket, typename U = T, typename = std::enable_if_t<std::is_base_of_v<PacketNetPacket, T> && std::is_base_of_v<PacketNetPacket, U>>>
   class PacketNetClient : public StreamedNetClient {
   public:
      using StreamedNetClient::StreamedNetClient;

      void sendHandshake(const U& pkt) {
         if(!firstSend) return;
         send(pkt.serialize());
         firstSend = false;
      }

      void sendPacket(const T& pkt) {
         if(firstSend) {
            U handshakePacket = pkt;
            send(handshakePacket.serialize());
            firstSend = false;
         } else {
            send(pkt.serialize());
         }
      }

   protected:
      void onReceive(const std::vector<ubyte_8>& data) override {
         incoming.insert(incoming.end(), data.begin(), data.end());
         processPackets();
      }

      virtual void onHandshake(const T& pkt) {}
      virtual void onPacket(const U& pkt) {}

      bool firstReceive = true;
      bool firstSend = true;

   private:
      std::vector<ubyte_8> incoming;

      void processPackets() {
         while (true) {
            if(firstReceive) {
               T handShakeP;

               if(!handShakeP.deserializeHead(incoming)) break;
               if(!handShakeP.checkHead()) { this->disconnect(); incoming.clear(); return; }
               if(!handShakeP.deserializeEnd(incoming)) break;
               if(!handShakeP.checkEnd()) { this->disconnect(); incoming.clear(); return; }

               handShakeP.readData();
               onHandshake(handShakeP);
               handShakeP.erase(incoming);

               firstReceive = false;
            } else {
               U dataP;

               if(!dataP.deserializeHead(incoming)) break;
               if(!dataP.checkHead()) { this->disconnect(); incoming.clear(); return; }
               if(!dataP.deserializeEnd(incoming)) break;
               if(!dataP.checkEnd()) { this->disconnect(); incoming.clear(); return; }

               dataP.readData();
               onPacket(dataP);
               dataP.erase(incoming);
            } 
         }
      }
   };

   template <typename T = DefaultPacket, typename U = T, typename = std::enable_if_t<std::is_base_of_v<PacketNetPacket, T> && std::is_base_of_v<PacketNetPacket, U>>>
   class PacketNetConnection : public StreamedNetConnection {
   public:
      using StreamedNetConnection::StreamedNetConnection;

      void sendHandshake(const U& pkt) {
         if(!firstSend) return;
         send(pkt.serialize());
         firstSend = false;
      }

      void sendPacket(const T& pkt) {
         if(firstSend) {
            U handshakePacket = pkt;
            send(handshakePacket.serialize());
            firstSend = false;
         } else {
            send(pkt.serialize());
         }
      }


   protected:
      void onReceive(const std::vector<ubyte_8>& data) override {
         incoming.insert(incoming.end(), data.begin(), data.end());
         processPackets();
      }

      virtual void onHandshake(const T& pkt) {}
      virtual void onPacket(const U& pkt) {}

      bool firstReceive = true;
      bool firstSend = true;

   private:
      std::vector<ubyte_8> incoming;

      void processPackets() {
         while (true) {
            if(firstReceive) {
               T handShakeP;

               if(!handShakeP.deserializeHead(incoming)) break;
               if(!handShakeP.checkHead()) { this->disconnect(); incoming.clear(); return; }
               if(!handShakeP.deserializeEnd(incoming)) break;
               if(!handShakeP.checkEnd()) { this->disconnect(); incoming.clear(); return; }

               handShakeP.readData();
               onHandshake(handShakeP);
               handShakeP.erase(incoming);

               firstReceive = false;
            } else {
               U dataP;

               if(!dataP.deserializeHead(incoming)) break;
               if(!dataP.checkHead()) { this->disconnect(); incoming.clear(); return; }
               if(!dataP.deserializeEnd(incoming)) break;
               if(!dataP.checkEnd()) { this->disconnect(); incoming.clear(); return; }

               dataP.readData();
               onPacket(dataP);
               dataP.erase(incoming);
            } 
         }
      }
   };

   class PacketNetServer : public StreamedNetServer {
   public:
      using StreamedNetServer::StreamedNetServer;
   };
}

#endif // NETWORK_PACKET_NET_TEMPLATED_H
