#include "system/Json.hpp"
#include <cctype>
#include <charconv>
#include <stdexcept>

namespace circuit::system {

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text_(text) {}
    Json document() {
        auto value = parseValue();
        skip();
        if (at_ != text_.size()) fail("text after the end of the document");
        return value;
    }

private:
    const std::string& text_;
    size_t at_ = 0;

    [[noreturn]] void fail(const std::string& what) const {
        throw std::invalid_argument("JSON: " + what + " at offset " + std::to_string(at_));
    }
    void skip() { while (at_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[at_]))) ++at_; }
    bool consume(char c) { skip(); if (at_ < text_.size() && text_[at_] == c) { ++at_; return true; } return false; }
    void expect(char c) { if (!consume(c)) fail(std::string("expected '") + c + "'"); }
    bool word(const char* w) {
        const std::string literal(w);
        if (text_.compare(at_, literal.size(), literal) == 0) { at_ += literal.size(); return true; }
        return false;
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (at_ < text_.size() && text_[at_] != '"') {
            char c = text_[at_++];
            if (c == '\\') {
                if (at_ >= text_.size()) fail("unfinished escape");
                const char e = text_[at_++];
                switch (e) {
                case '"': case '\\': case '/': out += e; break;
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (at_ + 4 > text_.size()) fail("short \\u escape");
                    unsigned code = 0;
                    const auto result = std::from_chars(text_.data() + at_, text_.data() + at_ + 4, code, 16);
                    if (result.ec != std::errc{} || result.ptr != text_.data() + at_ + 4) fail("bad \\u escape");
                    at_ += 4;
                    if (code > 0x7f) fail("only ASCII \\u escapes are supported");
                    out += static_cast<char>(code);
                    break;
                }
                default: fail("unknown escape");
                }
            } else {
                out += c;
            }
        }
        if (at_ >= text_.size()) fail("unfinished string");
        ++at_;
        return out;
    }

    Json parseValue() {
        skip();
        if (at_ >= text_.size()) fail("unexpected end");
        Json value;
        const char c = text_[at_];
        if (c == '{') {
            ++at_;
            value.kind_ = Json::Kind::Object;
            if (consume('}')) return value;
            do {
                skip();
                auto key = parseString();
                for (const auto& [existing, _] : value.members_)
                    if (existing == key) fail("duplicate key '" + key + "'");
                expect(':');
                value.members_.emplace_back(std::move(key), parseValue());
            } while (consume(','));
            expect('}');
        } else if (c == '[') {
            ++at_;
            value.kind_ = Json::Kind::Array;
            if (consume(']')) return value;
            do value.items_.push_back(parseValue()); while (consume(','));
            expect(']');
        } else if (c == '"') {
            value.kind_ = Json::Kind::String;
            value.text_ = parseString();
        } else if (word("true")) {
            value.kind_ = Json::Kind::Bool; value.bool_ = true;
        } else if (word("false")) {
            value.kind_ = Json::Kind::Bool; value.bool_ = false;
        } else if (word("null")) {
            value.kind_ = Json::Kind::Null;
        } else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            const size_t start = at_;
            ++at_;
            while (at_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[at_])) || text_[at_] == '.'
                   || text_[at_] == '+' || text_[at_] == '-')) ++at_;
            value.kind_ = Json::Kind::Number;
            value.text_ = text_.substr(start, at_ - start);
        } else {
            fail(std::string("unexpected character '") + c + "'");
        }
        return value;
    }
};

Json Json::parse(const std::string& text) { return JsonParser(text).document(); }

uint64_t parseUnsigned(const std::string& text, const std::string& where) {
    const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const char* first = text.data() + (hex ? 2 : 0);
    const char* last = text.data() + text.size();
    uint64_t value = 0;
    const auto result = std::from_chars(first, last, value, hex ? 16 : 10);
    if (text.empty() || first == last || result.ec != std::errc{} || result.ptr != last)
        throw std::invalid_argument(where + ": not an unsigned integer: '" + text + "'");
    return value;
}

bool Json::boolean(const std::string& where) const {
    if (kind_ != Kind::Bool) throw std::invalid_argument(where + ": expected true or false");
    return bool_;
}
const std::string& Json::string(const std::string& where) const {
    if (kind_ != Kind::String) throw std::invalid_argument(where + ": expected a string");
    return text_;
}
uint64_t Json::unsignedInteger(const std::string& where) const {
    if (kind_ != Kind::Number && kind_ != Kind::String) throw std::invalid_argument(where + ": expected an unsigned integer");
    return parseUnsigned(text_, where);
}
const std::vector<Json>& Json::array(const std::string& where) const {
    if (kind_ != Kind::Array) throw std::invalid_argument(where + ": expected an array");
    return items_;
}
const std::vector<std::pair<std::string, Json>>& Json::object(const std::string& where) const {
    if (kind_ != Kind::Object) throw std::invalid_argument(where + ": expected an object");
    return members_;
}
const Json* Json::find(const std::string& key) const {
    if (kind_ != Kind::Object) return nullptr;
    for (const auto& [name, value] : members_) if (name == key) return &value;
    return nullptr;
}
const Json& Json::at(const std::string& key) const {
    if (const auto* value = find(key)) return *value;
    throw std::invalid_argument("missing member '" + key + "'");
}

namespace {
std::string quoted(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out + "\"";
}
}

std::string Json::dump() const {
    switch (kind_) {
    case Kind::Null: return "null";
    case Kind::Bool: return bool_ ? "true" : "false";
    case Kind::Number: return text_;
    case Kind::String: return quoted(text_);
    case Kind::Array: {
        std::string out = "[";
        for (size_t i = 0; i < items_.size(); ++i) out += (i ? "," : "") + items_[i].dump();
        return out + "]";
    }
    case Kind::Object: {
        std::string out = "{";
        for (size_t i = 0; i < members_.size(); ++i) out += (i ? "," : "") + quoted(members_[i].first) + ":" + members_[i].second.dump();
        return out + "}";
    }
    }
    return "null";
}

} // namespace circuit::system
