#ifndef NODE_CONVERT_H_62B23520_7C8E_11DE_8A39_0800200C9A66
#define NODE_CONVERT_H_62B23520_7C8E_11DE_8A39_0800200C9A66

#if defined(_MSC_VER) ||                                            \
    (defined(__GNUC__) && (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || \
     (__GNUC__ >= 4))  // GCC supports "pragma once" correctly since 3.4
#pragma once
#endif

#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

#include "yaml-cpp/binary.h"
#include "yaml-cpp/node/impl.h"
#include "yaml-cpp/node/iterator.h"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/node/type.h"
#include "yaml-cpp/null.h"

namespace YAML {
class Binary;
struct _Null;
template <typename T, typename Enable>
struct convert;

}  // namespace YAML

namespace YAML {
namespace conversion {

static const std::regex re_true("true|True|TRUE");
static const std::regex re_false("false|False|FALSE");
static const std::regex re_decimal("[-+]?[0-9]+");
static const std::regex re_octal("0o[0-7]+");
static const std::regex re_hex("0x[0-9a-fA-F]+");
static const std::regex re_float(
    "[-+]?(\\.[0-9]+|[0-9]+(\\.[0-9]*)?)([eE][-+]?[0-9]+)?");
static const std::regex re_inf("[-+]?(\\.inf|\\.Inf|\\.INF)");
static const std::regex re_nan("\\.nan|\\.NaN|\\.NAN");

template <typename T>
struct is_character {
  using value_type = bool;
  static constexpr bool value =
      (std::is_same<T, char>::value || std::is_same<T, wchar_t>::value ||
       std::is_same<T, char16_t>::value || std::is_same<T, char32_t>::value);
  using type = std::integral_constant<bool, value>;
};
}

// Node
template <>
struct convert<Node> {
  static Node encode(const Node& rhs) { return rhs; }

  static bool decode(const Node& node, Node& rhs) {
    rhs.reset(node);
    return true;
  }
};

// std::string
template <>
struct convert<std::string> {
  static Node encode(const std::string& rhs) { return Node(rhs); }

  static bool decode(const Node& node, std::string& rhs) {
    if (!node.IsScalar())
      return false;
    rhs = node.Scalar();
    return true;
  }
};

// C-strings can only be encoded
template <>
struct convert<const char*> {
  static Node encode(const char*& rhs) { return Node(rhs); }
};

template <std::size_t N>
struct convert<const char[N]> {
  static Node encode(const char (&rhs)[N]) { return Node(rhs); }
};

template <>
struct convert<_Null> {
  static Node encode(const _Null& /* rhs */) { return Node(); }

  static bool decode(const Node& node, _Null& /* rhs */) {
    return node.IsNull();
  }
};

// character types
template <typename type>
struct convert<type, typename std::enable_if<
                         conversion::is_character<type>::value>::type> {
  static Node encode(const type& rhs) { return Node(std::string(1,rhs)); }

  static bool decode(const Node& node, type& rhs) {
    if (node.Type() != NodeType::Scalar)
      return false;
    const std::string& input = node.Scalar();
    if (input.length() != 1) {
      return false;
    }
    rhs = input[0];
    return true;
  }
};

// signed integral (sans char)
template <typename type>
struct convert<
    type, typename std::enable_if<
              std::is_integral<type>::value && std::is_signed<type>::value &&
              !conversion::is_character<type>::value>::type> {
  static Node encode(const type& rhs) { return Node(std::to_string(rhs)); }

  static bool decode(const Node& node, type& rhs) {
    if (node.Type() != NodeType::Scalar)
      return false;
    const std::string& input = node.Scalar();
    long long num;
    if (std::regex_match(input, conversion::re_decimal)) {
      try {
        num = std::stoll(input);
      } catch (const std::out_of_range&) {
        return false;
      }
    } else if (std::regex_match(input, conversion::re_octal)) {
      try {
        num = std::stoll(input.substr(2), nullptr, 8);
      } catch (const std::out_of_range&) {
        return false;
      }
    } else if (std::regex_match(input, conversion::re_hex)) {
      try {
        num = std::stoll(input.substr(2), nullptr, 16);
      } catch (const std::out_of_range&) {
        return false;
      }
    } else {
      return false;
    }
    if (num > std::numeric_limits<type>::max() ||
        num < std::numeric_limits<type>::min()) {
      return false;
    }
    rhs = num;
    return true;
  }
};

// unsigned integral (sans char)
template <typename type>
struct convert<
    type, typename std::enable_if<
              std::is_integral<type>::value && std::is_unsigned<type>::value &&
              !conversion::is_character<type>::value>::type> {
  static Node encode(const type& rhs) { return Node(std::to_string(rhs)); }

  static bool decode(const Node& node, type& rhs) {
    if (node.Type() != NodeType::Scalar)
      return false;
    const std::string& input = node.Scalar();
    unsigned long long num;
    if (std::regex_match(input, conversion::re_decimal)) {
      try {
        num = std::stoull(input);
      } catch (const std::out_of_range&) {
        return false;
      }
    } else if (std::regex_match(input, conversion::re_octal)) {
      try {
        num = std::stoull(input.substr(2), nullptr, 8);
      } catch (const std::out_of_range&) {
        return false;
      }
    } else if (std::regex_match(input, conversion::re_hex)) {
      try {
        num = std::stoull(input.substr(2), nullptr, 16);
      } catch (const std::out_of_range&) {
        return false;
      }
    } else {
      return false;
    }
    if (num > std::numeric_limits<type>::max() ||
        num < std::numeric_limits<type>::min()) {
      return false;
    }
    rhs = num;
    return true;
  }
};

// floating point
template <typename type>
struct convert<
    type, typename std::enable_if<std::is_floating_point<type>::value>::type> {
  static Node encode(const type& rhs) {
    if (std::isnan(rhs)) {
      return Node(".nan");
    }
    if (std::isinf(rhs)) {
      if (std::signbit(rhs)) {
        return Node("-.inf");
      }
      return Node(".inf");
    }
    std::stringstream stream;
    stream.precision(std::numeric_limits<type>::max_digits10);
    stream << rhs;
    auto str = stream.str();
    if (std::regex_match(str, conversion::re_decimal)) {
      return Node(str + ".");  // Disambiguate float from int
    }
    return Node(str);
  }

  static bool decode(const Node& node, type& rhs) {
    if (node.Type() != NodeType::Scalar)
      return false;
    const std::string& input = node.Scalar();
    long double num;
    if (std::regex_match(input, conversion::re_float)) {
      try {
        num = std::stold(input);
      } catch (const std::out_of_range&) {
        return false;
      }
    } else if (std::regex_match(input, conversion::re_inf)) {
      if (input[0] == '-') {
        num = -std::numeric_limits<long double>::infinity();
      } else {
        num = std::numeric_limits<long double>::infinity();
      }
    } else if (std::regex_match(input, conversion::re_nan)) {
      num = std::numeric_limits<long double>::quiet_NaN();
    } else {
      return false;
    }
    rhs = num;
    return true;
  }
};

// bool
template <>
struct convert<bool> {
  static Node encode(bool rhs) { return rhs ? Node("true") : Node("false"); }

  YAML_CPP_API static bool decode(const Node& node, bool& rhs);
};

// std::map
template <typename K, typename V>
struct convert<std::map<K, V>> {
  static Node encode(const std::map<K, V>& rhs) {
    Node node(NodeType::Map);
    for (typename std::map<K, V>::const_iterator it = rhs.begin();
         it != rhs.end(); ++it)
      node.force_insert(it->first, it->second);
    return node;
  }

  static bool decode(const Node& node, std::map<K, V>& rhs) {
    if (!node.IsMap())
      return false;

    rhs.clear();
    for (const_iterator it = node.begin(); it != node.end(); ++it)
#if defined(__GNUC__) && __GNUC__ < 4
      // workaround for GCC 3:
      rhs[it->first.template as<K>()] = it->second.template as<V>();
#else
      rhs[it->first.as<K>()] = it->second.as<V>();
#endif
    return true;
  }
};

// std::vector
template <typename T>
struct convert<std::vector<T>> {
  static Node encode(const std::vector<T>& rhs) {
    Node node(NodeType::Sequence);
    for (typename std::vector<T>::const_iterator it = rhs.begin();
         it != rhs.end(); ++it)
      node.push_back(*it);
    return node;
  }

  static bool decode(const Node& node, std::vector<T>& rhs) {
    if (!node.IsSequence())
      return false;

    rhs.clear();
    for (const_iterator it = node.begin(); it != node.end(); ++it)
#if defined(__GNUC__) && __GNUC__ < 4
      // workaround for GCC 3:
      rhs.push_back(it->template as<T>());
#else
      rhs.push_back(it->as<T>());
#endif
    return true;
  }
};

// std::list
template <typename T>
struct convert<std::list<T>> {
  static Node encode(const std::list<T>& rhs) {
    Node node(NodeType::Sequence);
    for (typename std::list<T>::const_iterator it = rhs.begin();
         it != rhs.end(); ++it)
      node.push_back(*it);
    return node;
  }

  static bool decode(const Node& node, std::list<T>& rhs) {
    if (!node.IsSequence())
      return false;

    rhs.clear();
    for (const_iterator it = node.begin(); it != node.end(); ++it)
#if defined(__GNUC__) && __GNUC__ < 4
      // workaround for GCC 3:
      rhs.push_back(it->template as<T>());
#else
      rhs.push_back(it->as<T>());
#endif
    return true;
  }
};

// std::array
template <typename T, std::size_t N>
struct convert<std::array<T, N>> {
  static Node encode(const std::array<T, N>& rhs) {
    Node node(NodeType::Sequence);
    for (const auto& element : rhs) {
      node.push_back(element);
    }
    return node;
  }

  static bool decode(const Node& node, std::array<T, N>& rhs) {
    if (!isNodeValid(node)) {
      return false;
    }

    for (auto i = 0u; i < node.size(); ++i) {
#if defined(__GNUC__) && __GNUC__ < 4
      // workaround for GCC 3:
      rhs[i] = node[i].template as<T>();
#else
      rhs[i] = node[i].as<T>();
#endif
    }
    return true;
  }

 private:
  static bool isNodeValid(const Node& node) {
    return node.IsSequence() && node.size() == N;
  }
};

// std::pair
template <typename T, typename U>
struct convert<std::pair<T, U>> {
  static Node encode(const std::pair<T, U>& rhs) {
    Node node(NodeType::Sequence);
    node.push_back(rhs.first);
    node.push_back(rhs.second);
    return node;
  }

  static bool decode(const Node& node, std::pair<T, U>& rhs) {
    if (!node.IsSequence())
      return false;
    if (node.size() != 2)
      return false;

#if defined(__GNUC__) && __GNUC__ < 4
    // workaround for GCC 3:
    rhs.first = node[0].template as<T>();
#else
    rhs.first = node[0].as<T>();
#endif
#if defined(__GNUC__) && __GNUC__ < 4
    // workaround for GCC 3:
    rhs.second = node[1].template as<U>();
#else
    rhs.second = node[1].as<U>();
#endif
    return true;
  }
};

// binary
template <>
struct convert<Binary> {
  static Node encode(const Binary& rhs) {
    return Node(EncodeBase64(rhs.data(), rhs.size()));
  }

  static bool decode(const Node& node, Binary& rhs) {
    if (!node.IsScalar())
      return false;

    std::vector<unsigned char> data = DecodeBase64(node.Scalar());
    if (data.empty() && !node.Scalar().empty())
      return false;

    rhs.swap(data);
    return true;
  }
};
}

#endif  // NODE_CONVERT_H_62B23520_7C8E_11DE_8A39_0800200C9A66
