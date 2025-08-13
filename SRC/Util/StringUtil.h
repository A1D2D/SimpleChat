#ifndef PORTBRIDGE_STRINGUTIL_H
#define PORTBRIDGE_STRINGUTIL_H

#include <VORTEX_MP/LinearAlg>

#include <charconv>
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

class StringUtil {
public:
   static std::vector<std::string> split(const std::string& input, const std::string& delimiter) {
      std::vector<std::string> result;
      size_t start = 0;
      size_t end;

      while ((end = input.find(delimiter, start)) != std::string::npos) {
         result.push_back(input.substr(start, end - start));
         start = end + delimiter.length();
      }
      result.push_back(input.substr(start)); // last part
      return result;
   }

   static int toIntOrFail(const std::string& str, std::string& errorMsg) {
      try {
         size_t idx;
         int value = std::stoi(str, &idx);

         if (idx != str.length()) {
            throw std::invalid_argument("Trailing characters after number");
         }

         return value;
      } catch (const std::exception& e) {
         errorMsg = "Conversion failed: " + std::string(e.what()) + " ill-formed port\n";
      }
      return -1;
   }

   static std::string trim(const std::string& str) {
      size_t first = str.find_first_not_of(" \n\t\r");
      if (first == std::string::npos) return "";

      size_t last = str.find_last_not_of(" \n\t\r");
      return str.substr(first, last - first + 1);
   }

   static std::string suggestIDdName(const std::vector<std::string>& existingTunnels, const std::string& name) {
      int id = 1;
      while (true) {
         std::string candidate = name + std::to_string(id);
         if (std::ranges::find(existingTunnels.begin(), existingTunnels.end(), candidate) == existingTunnels.end())
            return candidate;
         ++id;
      }
   }

   static size_t utf8_char_count(const std::string& s) {
      size_t count = 0;
      for (size_t i = 0; i < s.size(); ) {
         unsigned char c = s[i];
         if ((c & 0x80) == 0) {
            i += 1;
         } else if ((c & 0xE0) == 0xC0) {
            i += 2;
         } else if ((c & 0xF0) == 0xE0) {
            i += 3;
         } else if ((c & 0xF8) == 0xF0) {
            i += 4;
         } else {
            i += 1;
         }
         count++;
      }
      return count;
   }

   static std::string getLastUTF8Char(const std::string& str) {
      if (str.empty()) return "";

      size_t i = str.size() - 1;

      while (i > 0 && (static_cast<unsigned char>(str[i]) & 0xC0) == 0x80) {
         i--;
      }

      return str.substr(i);
   }

   static void eraseLastUTF8Char(std::string& str) {
      if (str.empty()) return;

      size_t i = str.size() - 1;

      while (i > 0 && (static_cast<unsigned char>(str[i]) & 0xC0) == 0x80) {
         i--;
      }
      str.erase(i);
   }

   static std::string utf8Truncate(const std::string& input, size_t max_chars) {
      std::string output;
      output.reserve(input.size());

      size_t char_count = 0;
      const unsigned char* ptr = reinterpret_cast<const unsigned char*>(input.data());
      const unsigned char* end = ptr + input.size();

      while (ptr < end && char_count < max_chars) {
         unsigned char c = *ptr;
         size_t char_len = 0;

         if (c < 0x80) {               // 1-byte sequence
            char_len = 1;
         } else if ((c >> 5) == 0x6) { // 2-byte sequence
            char_len = 2;
         } else if ((c >> 4) == 0xE) { // 3-byte sequence
            char_len = 3;
         } else if ((c >> 3) == 0x1E) { // 4-byte sequence
            char_len = 4;
         } else {
            ++ptr;
            continue;
         }

         if (ptr + char_len > end) break;

         output.append(reinterpret_cast<const char*>(ptr), char_len);
         ptr += char_len;
         ++char_count;
      }

      return output;
   }

   static std::vector<ubyte_8> stringToBytes(const std::string& str) {
      return std::vector<ubyte_8>(str.begin(), str.end());
   }

   static std::string bytesToString(const std::vector<ubyte_8>& bytes) {
      return std::string(bytes.begin(), bytes.end());
   }

   static bool containsAny(const std::string& text, const std::vector<std::string>& words) {
      for (const auto& word : words) {
         if (text.find(word) != std::string::npos) {
            return true;
         }
      }
      return false;
   }

   // ---------- Base Template ----------
   template<typename T>
   static std::optional<T> parseArg(const std::vector<std::string>& args, size_t index);

   // ---------- Integer & Floating Point ----------
   template<typename T>
   requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>)
   static std::optional<T> parseArg(const std::vector<std::string>& args, size_t index) {
      if (index >= args.size()) return std::nullopt;
      const std::string_view str = args[index];

      T value{};
      auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
      if (ec != std::errc{} || ptr != str.data() + str.size())
         return std::nullopt;
      return value;
   }

   // ---------- Boolean ----------
   template<>
   inline std::optional<bool> parseArg<bool>(const std::vector<std::string>& args, size_t index) {
      if (index >= args.size()) return std::nullopt;
      const std::string& str = args[index];

      if (str == "1" || str == "true" || str == "on") return true;
      if (str == "0" || str == "false" || str == "off") return false;
      return std::nullopt;
   }

   // ---------- Char ----------
   template<>
   inline std::optional<char> parseArg<char>(const std::vector<std::string>& args, size_t index) {
      if (index >= args.size() || args[index].empty()) return std::nullopt;
      return args[index][0];
   }

   // ---------- String ----------
   template<>
   inline std::optional<std::string> parseArg<std::string>(const std::vector<std::string>& args, size_t index) {
      if (index >= args.size()) return std::nullopt;
      return args[index];
   }
};

#endif //PORTBRIDGE_STRINGUTIL_H
