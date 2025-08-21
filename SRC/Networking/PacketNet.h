#ifndef NETWORK_PACKET_NET_TEMPLATED_H
#define NETWORK_PACKET_NET_TEMPLATED_H

#include "StreamedNet.h"
#include <vector>
#include <cstring>

namespace PN {
   using namespace SN;

   template<typename T>
   class PacketNetClient : public StreamedNetClient {
   public:
      using StreamedNetClient::StreamedNetClient;

      void sendPacket(const T& pkt) {
         std::vector<ubyte_8> buffer = serialize(pkt);
         send(buffer);
      }

   protected:
      void onReceive(const std::vector<ubyte_8>& data) override {
         incoming.insert(incoming.end(), data.begin(), data.end());
         processPackets();
      }

      virtual void onPacket(const T& pkt) {}

   private:
      std::vector<ubyte_8> incoming;

      void processPackets() {
         while (incoming.size() >= 4) {
            uint32_t len;
            std::memcpy(&len, incoming.data(), 4);
            if (incoming.size() < 4 + len) break;

            std::vector<ubyte_8> raw(incoming.begin() + 4, incoming.begin() + 4 + len);
            T pkt = deserialize(raw);
            incoming.erase(incoming.begin(), incoming.begin() + 4 + len);
            onPacket(pkt);
         }
      }

      static std::vector<ubyte_8> serialize(const T& pkt) {
         std::vector<ubyte_8> buf(sizeof(T));
         std::memcpy(buf.data(), &pkt, sizeof(T));
         std::vector<ubyte_8> result(4 + buf.size());
         uint32_t len = static_cast<uint32_t>(buf.size());
         std::memcpy(result.data(), &len, 4);
         std::memcpy(result.data() + 4, buf.data(), buf.size());
         return result;
      }

      static T deserialize(const std::vector<ubyte_8>& buf) {
         T pkt;
         std::memcpy(&pkt, buf.data(), sizeof(T));
         return pkt;
      }
   };

   template<typename T>
   class PacketNetConnection : public StreamedNetConnection {
   public:
      using StreamedNetConnection::StreamedNetConnection;

      void sendPacket(const T& pkt) {
         std::vector<ubyte_8> buffer = PacketNetClient<T>::serialize(pkt);
         send(buffer);
      }

   protected:
      void onReceive(const std::vector<ubyte_8>& data) override {
         incoming.insert(incoming.end(), data.begin(), data.end());
         processPackets();
      }

      virtual void onPacket(const T& pkt) {}

   private:
      std::vector<ubyte_8> incoming;

      void processPackets() {
         while (incoming.size() >= 4) {
            uint32_t len;
            std::memcpy(&len, incoming.data(), 4);
            if (incoming.size() < 4 + len) break;

            std::vector<ubyte_8> raw(incoming.begin() + 4, incoming.begin() + 4 + len);
            T pkt = PacketNetClient<T>::deserialize(raw);
            incoming.erase(incoming.begin(), incoming.begin() + 4 + len);
            onPacket(pkt);
         }
      }
   };

   template<typename T>
   class PacketNetServer : public StreamedNetServer {
   public:
      using StreamedNetServer::StreamedNetServer;
   };
}

#endif // NETWORK_PACKET_NET_TEMPLATED_H
