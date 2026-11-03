#pragma once
// A small JSON reader for system descriptions: objects keep their key order,
// numbers are kept as their text so addresses such as "0x10000000" (given as
// strings) and plain integers both read exactly.
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace circuit::system {

class Json {
public:
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Json() = default;
    static Json parse(const std::string& text);

    Kind kind() const { return kind_; }
    bool isNull() const { return kind_ == Kind::Null; }
    bool isObject() const { return kind_ == Kind::Object; }
    bool isArray() const { return kind_ == Kind::Array; }
    bool isString() const { return kind_ == Kind::String; }

    // Typed reads; each throws std::invalid_argument naming `where` on a
    // wrong kind.
    bool boolean(const std::string& where) const;
    const std::string& string(const std::string& where) const;
    // A non-negative integer: a JSON number, or a string in decimal or 0x hex.
    uint64_t unsignedInteger(const std::string& where) const;
    const std::vector<Json>& array(const std::string& where) const;
    const std::vector<std::pair<std::string, Json>>& object(const std::string& where) const;

    // Object members: `at` throws when absent, `find` returns null.
    const Json& at(const std::string& key) const;
    const Json* find(const std::string& key) const;
    bool has(const std::string& key) const { return find(key) != nullptr; }

    // Compact text form (keys in their order), for parameters and fingerprints.
    std::string dump() const;

private:
    Kind kind_ = Kind::Null;
    bool bool_ = false;
    std::string text_;  // string value, or a number's text
    std::vector<Json> items_;
    std::vector<std::pair<std::string, Json>> members_;
    friend class JsonParser;
};

// Parses a decimal or 0x-prefixed hexadecimal unsigned integer exactly.
uint64_t parseUnsigned(const std::string& text, const std::string& where);

} // namespace circuit::system
