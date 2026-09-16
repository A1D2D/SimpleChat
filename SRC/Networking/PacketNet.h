#ifndef NCORE_PACKET_NET_TEMPLATED_H
#define NCORE_PACKET_NET_TEMPLATED_H

#include <map>
#include <string>
#include <vector>

namespace PN {
   #define SERIALIZABLE(...) \
      std::vector<uint8_t> serialize() const override { std::vector<uint8_t> buf; PN::serializeFields(buf, __VA_ARGS__);  return buf; } \
      bool deserialize(const std::vector<uint8_t>& buf, size_t& offset) override { return PN::deserializeFields(buf, offset, __VA_ARGS__); }

   struct Serializable {
      virtual std::vector<uint8_t> serialize() const = 0;
      virtual bool deserialize(const std::vector<uint8_t>& buf, size_t& offset) = 0;
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
   inline void writeAny(std::vector<uint8_t>& buf, const T& v) {
      if constexpr (std::is_same_v<T, std::string>) {
         writeAny(buf, static_cast<uint64_t>(v.size()));
         buf.insert(buf.end(), v.begin(), v.end());
      }
      else if constexpr (SerializableType<T>) {
         auto tmp = v.serialize();
         writeAny(buf, tmp);
      }
      else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
         writeAny(buf, static_cast<uint64_t>(v.size()));
         buf.insert(buf.end(), v.begin(), v.end());
      }
      else if constexpr (is_specialization<T, std::vector>::value) {
         writeAny(buf, static_cast<uint64_t>(v.size()));
         for (auto& e : v) writeAny(buf, e);
      }
      else if constexpr (is_specialization<T, std::map>::value) {
         writeAny(buf, static_cast<uint64_t>(v.size()));
         for (auto& [k, val] : v) {
            writeAny(buf, k);
            writeAny(buf, val);
         }
      }
      else if constexpr (std::is_trivially_copyable_v<T>) {
         auto ptr = reinterpret_cast<const uint8_t*>(&v);
         buf.insert(buf.end(), ptr, ptr + sizeof(T));
      }
      else {
         static_assert(sizeof(T) == 0, "Unsupported type for writeAny");
      }
   }

   template<typename T>
   inline bool readAny(const std::vector<uint8_t>& buf, uint64_t& offset, T& v) {
      if constexpr (std::is_same_v<T, std::string>) {
         uint64_t size;
         if (!readAny(buf, offset, size)) return false;
         if (offset + size > buf.size()) return false;
         v.assign(reinterpret_cast<const char*>(buf.data() + offset), size);
         offset += size;
         return true;
      }
      else if constexpr (SerializableType<T>) {
         std::vector<uint8_t> tmp;
         if (!readAny(buf, offset, tmp)) return false;
         uint64_t innerOffset = 0;
         return v.deserialize(tmp, innerOffset);
      }
      else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
         uint64_t size;
         if (!readAny(buf, offset, size)) return false;
         if (offset + size > buf.size()) return false;
         v.assign(buf.begin() + offset, buf.begin() + offset + size);
         offset += size;
         return true;
      }
      else if constexpr (is_specialization<T, std::vector>::value) {
         uint64_t size;
         if (!readAny(buf, offset, size)) return false;
         v.clear();
         v.reserve(size);
         for (uint64_t i = 0; i < size; i++) {
            typename T::value_type tmp;
            if (!readAny(buf, offset, tmp)) return false;
            v.push_back(std::move(tmp));
         }
         return true;
      }
      else if constexpr (is_specialization<T, std::map>::value) {
         uint64_t size;
         if (!readAny(buf, offset, size)) return false;
         v.clear();
         for (uint64_t i = 0; i < size; i++) {
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

   inline void serializeFields(std::vector<uint8_t>&) {}

   template<typename T, typename... Rest>
   inline void serializeFields(std::vector<uint8_t>& buf, const T& first, const Rest&... rest) {
      writeAny(buf, first);
      serializeFields(buf, rest...);
   }

   inline bool deserializeFields(const std::vector<uint8_t>&, uint64_t&) { return true; }

   template<typename T, typename... Rest>
   inline bool deserializeFields(const std::vector<uint8_t>& buf, uint64_t& offset, T& first, Rest&... rest) {
      if (!readAny(buf, offset, first)) return false;
      return deserializeFields(buf, offset, rest...);
   }
}

#endif //~NCORE_PACKET_NET_TEMPLATED_H