#ifndef NETWORK_PACKET_NET_TEMPLATED_H
#define NETWORK_PACKET_NET_TEMPLATED_H

#include <type_traits>
#include <vector>
#include <cstring>
#include <map>

#include "StreamedNet.h"

#define SERIALIZABLE(...) \
   std::vector<ubyte_8> serialize() const override { std::vector<ubyte_8> buf; PN::serializeFields(buf, __VA_ARGS__);  return buf; } \
   bool deserialize(const std::vector<ubyte_8>& buf, size_t& offset) override { return PN::deserializeFields(buf, offset, __VA_ARGS__); }

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

   struct Serializable {
      virtual std::vector<ubyte_8> serialize() const = 0;
      virtual bool deserialize(const std::vector<ubyte_8>& buf, size_t& offset) = 0;
      virtual ~Serializable() = default;
   };

   template<typename T>
   concept SerializableType = std::is_base_of_v<Serializable, T>;

   template <typename T, template<typename...> class Template>
   struct is_specialization : std::false_type {};

   template <template<typename...> class Template, typename... Args>
   struct is_specialization<Template<Args...>, Template> : std::true_type {};

   //<POD types>
   template<typename T>
   inline void writeAny(std::vector<ubyte_8>& buf, const T& v) {
      if constexpr (std::is_same_v<T, std::string>) {
         writeAny(buf, static_cast<ulong_64>(v.size()));
         buf.insert(buf.end(), v.begin(), v.end());
      }
      else if constexpr (SerializableType<T>) {
         auto tmp = v.serialize();
         writeAny(buf, tmp);
      }
      else if constexpr (std::is_same_v<T, std::vector<ubyte_8>>) {
         writeAny(buf, static_cast<ulong_64>(v.size()));
         buf.insert(buf.end(), v.begin(), v.end());
      }
      else if constexpr (is_specialization<T, std::vector>::value) {
         writeAny(buf, static_cast<ulong_64>(v.size()));
         for (auto& e : v) writeAny(buf, e);
      }
      else if constexpr (is_specialization<T, std::map>::value) {
         writeAny(buf, static_cast<ulong_64>(v.size()));
         for (auto& [k, val] : v) {
            writeAny(buf, k);
            writeAny(buf, val);
         }
      }
      else if constexpr (std::is_trivially_copyable_v<T>) {
         auto ptr = reinterpret_cast<const ubyte_8*>(&v);
         buf.insert(buf.end(), ptr, ptr + sizeof(T));
      }
      else {
         static_assert(sizeof(T) == 0, "Unsupported type for writeAny");
      }
   }

   template<typename T>
   inline bool readAny(const std::vector<ubyte_8>& buf, ulong_64& offset, T& v) {
      if constexpr (std::is_same_v<T, std::string>) {
         ulong_64 size;
         if (!readAny(buf, offset, size)) return false;
         if (offset + size > buf.size()) return false;
         v.assign(reinterpret_cast<const char*>(buf.data() + offset), size);
         offset += size;
         return true;
      }
      else if constexpr (SerializableType<T>) {
         std::vector<ubyte_8> tmp;
         if (!readAny(buf, offset, tmp)) return false;
         ulong_64 innerOffset = 0;
         return v.deserialize(tmp, innerOffset);
      }
      else if constexpr (std::is_same_v<T, std::vector<ubyte_8>>) {
         ulong_64 size;
         if (!readAny(buf, offset, size)) return false;
         if (offset + size > buf.size()) return false;
         v.assign(buf.begin() + offset, buf.begin() + offset + size);
         offset += size;
         return true;
      }
      else if constexpr (is_specialization<T, std::vector>::value) {
         ulong_64 size;
         if (!readAny(buf, offset, size)) return false;
         v.clear();
         v.reserve(size);
         for (ulong_64 i = 0; i < size; i++) {
            typename T::value_type tmp;
            if (!readAny(buf, offset, tmp)) return false;
            v.push_back(std::move(tmp));
         }
         return true;
      }
      else if constexpr (is_specialization<T, std::map>::value) {
         ulong_64 size;
         if (!readAny(buf, offset, size)) return false;
         v.clear();
         for (ulong_64 i = 0; i < size; i++) {
            typename T::key_type key;
            typename T::mapped_type val;
            if (!readAny(buf, offset, key)) return false;
            if (!readAny(buf, offset, val)) return false;
            v.emplace(std::move(key), std::move(val));
         }
         return true;
      }
      else if constexpr (std::is_trivially_copyable_v<T>) {
         if (offset + sizeof(T) > buf.size()) return false;
         std::memcpy(&v, buf.data() + offset, sizeof(T));
         offset += sizeof(T);
         return true;
      }
      else {
         static_assert(sizeof(T) == 0, "Unsupported type for readAny");
      }
   }
   //<~POD types>

   inline void serializeFields(std::vector<ubyte_8>&) {}

   template<typename T, typename... Rest>
   inline void serializeFields(std::vector<ubyte_8>& buf, const T& first, const Rest&... rest) {
      writeAny(buf, first);
      serializeFields(buf, rest...);
   }

   inline bool deserializeFields(const std::vector<ubyte_8>&, ulong_64&) { return true; }

   template<typename T, typename... Rest>
   inline bool deserializeFields(const std::vector<ubyte_8>& buf, ulong_64& offset, T& first, Rest&... rest) {
      if (!readAny(buf, offset, first)) return false;
      return deserializeFields(buf, offset, rest...);
   }
   
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