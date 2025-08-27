#ifndef NETWORK_PACKET_NET_TEMPLATED_H
#define NETWORK_PACKET_NET_TEMPLATED_H

#include <type_traits>
#include <vector>
#include <cstring>

#include "StreamedNet.h"

namespace PN {
   using namespace SN;
   using ulong_64 = std::uint64_t;

   struct PacketNetPacket {
      PacketNetPacket() = default;
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
      constexpr static const ulong_64 maxPacketSize = 64*1024*1024;
      std::string_view readHeadSpecifier{};
      std::string_view readEndSpecifier{};
      ulong_64 len;
      std::vector<ubyte_8> data;

      DefaultPacket() : len(0) {}

      DefaultPacket(const std::vector<ubyte_8>& packetData) {
         data = packetData;
         len = data.size();
      }

      ~DefaultPacket() override {}

      bool deserializeHead(std::vector<ubyte_8>& incoming) override {
         if(incoming.size() < headSpecifier.size() + 8) return false;
         
         readHeadSpecifier = std::string_view(reinterpret_cast<char*>(incoming.data()), headSpecifier.size());
         std::memcpy(&len, incoming.data()+headSpecifier.size(), 8);

         return true;
      }

      bool checkHead() override {
         if(maxPacketSize != 0) {
            if(headSpecifier.size() + 8 + len + endSpecifier.size() > maxPacketSize) return false;
         }
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
         incoming.erase(incoming.begin(), incoming.begin() + headSpecifier.size() + 8 + len + endSpecifier.size());
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
   class PacketNetServer;


   template <typename T = DefaultPacket, typename U = T, typename = std::enable_if_t<std::is_base_of_v<PacketNetPacket, T> && std::is_base_of_v<PacketNetPacket, U>>>
   class PacketNetClient : public StreamedNetClient {
   public:
      using StreamedNetClient::StreamedNetClient;

      void sendHandshake() {
         sendHandshake(U());
      }

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
      virtual void onReceive(const std::vector<ubyte_8>& data) override {
         incoming.insert(incoming.end(), data.begin(), data.end());
         // std::cout << "FullPacket: " << StringUtil::bytesToString(incoming) << "\n";
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

               handShakeP.readData(incoming);
               onHandshake(handShakeP);
               handShakeP.erase(incoming);

               firstReceive = false;
            } else {
               U dataP;

               if(!dataP.deserializeHead(incoming)) break;
               if(!dataP.checkHead()) { this->disconnect(); incoming.clear(); return; }
               if(!dataP.deserializeEnd(incoming)) break;
               if(!dataP.checkEnd()) { this->disconnect(); incoming.clear(); return; }

               dataP.readData(incoming);
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

      PacketNetConnection(asio::io_context& context, PacketNetServer<T, U>& serverRef, tcp::socket& accepted) : StreamedNetConnection(context, serverRef, accepted) {}

      PacketNetServer<T, U>& getServer() {
         return static_cast<PacketNetServer<T, U>&>(StreamedNetConnection::getServer());
      }

      void sendHandshake() {
         sendHandshake(U());
      }

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
         // std::cout << "FullPacket: " << StringUtil::bytesToString(incoming) << "\n";
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

               handShakeP.readData(incoming);
               onHandshake(handShakeP);
               handShakeP.erase(incoming);

               firstReceive = false;
            } else {
               U dataP;

               if(!dataP.deserializeHead(incoming)) break;
               if(!dataP.checkHead()) { this->disconnect(); incoming.clear(); return; }
               if(!dataP.deserializeEnd(incoming)) break;
               if(!dataP.checkEnd()) { this->disconnect(); incoming.clear(); return; }

               dataP.readData(incoming);
               onPacket(dataP);
               dataP.erase(incoming);
            } 
         }
      }
   };


   template <typename T, typename U, typename>
   class PacketNetServer : public StreamedNetServer {
   public:
      using StreamedNetServer::StreamedNetServer;

      std::vector<std::shared_ptr<PacketNetConnection<T, U>>> getConnections() {
         std::vector<std::shared_ptr<PacketNetConnection<T, U>>> out;
         for (auto& conn : StreamedNetServer::getConnections()) {
            out.emplace_back(std::static_pointer_cast<PacketNetConnection<T, U>>(conn));
         }
         return out;
      }

      void onDisconnect(std::shared_ptr<StreamedNetConnection> connection) override {
         auto derivedConn = std::static_pointer_cast<PN::PacketNetConnection<T, U>>(connection);
         onDisconnect(derivedConn);
      }

      virtual void onDisconnect(std::shared_ptr<PN::PacketNetConnection<T, U>> connection) {}
   };
}

#endif // NETWORK_PACKET_NET_TEMPLATED_H