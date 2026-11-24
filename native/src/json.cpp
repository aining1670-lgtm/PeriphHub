#include "json.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace json {
namespace {

class Parser {
public:
    explicit Parser(const std::string& text, bool allow_float = false)
        : in_(text), allow_float_(allow_float) {}

    bool ParseDocument(Value* out, std::string* error) {
        if (in_.size() >= 3 &&
            static_cast<unsigned char>(in_[0]) == 0xEF &&
            static_cast<unsigned char>(in_[1]) == 0xBB &&
            static_cast<unsigned char>(in_[2]) == 0xBF) {
            pos_ = 3;
        }
        SkipSpace();
        if (!ParseValue(out)) {
            *error = error_;
            return false;
        }
        SkipSpace();
        if (pos_ != in_.size()) {
            *error = "trailing data at offset " + std::to_string(pos_);
            return false;
        }
        return true;
    }

private:
    bool Fail(const std::string& message) {
        if (error_.empty()) error_ = message + " at offset " + std::to_string(pos_);
        return false;
    }

    void SkipSpace() {
        while (pos_ < in_.size()) {
            const unsigned char c = static_cast<unsigned char>(in_[pos_]);
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            ++pos_;
        }
    }

    bool Literal(const char* text) {
        const std::size_t len = std::char_traits<char>::length(text);
        if (in_.compare(pos_, len, text) != 0) return false;
        pos_ += len;
        return true;
    }

    bool ParseValue(Value* out) {
        if (depth_ >= kMaxDepth) {
            return Fail("nesting depth exceeded limit");
        }
        struct DepthGuard {
            int& d;
            explicit DepthGuard(int& depth) : d(depth) { ++d; }
            ~DepthGuard() { --d; }
        } guard(depth_);

        SkipSpace();
        if (pos_ >= in_.size()) return Fail("unexpected end of input");
        const char c = in_[pos_];
        if (c == '{') return ParseObject(out);
        if (c == '[') return ParseArray(out);
        if (c == '"') {
            out->type = Value::Type::kString;
            return ParseString(&out->text);
        }
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out);
        if (Literal("true")) {
            out->type = Value::Type::kBool;
            out->boolean = true;
            return true;
        }
        if (Literal("false")) {
            out->type = Value::Type::kBool;
            out->boolean = false;
            return true;
        }
        if (Literal("null")) {
            out->type = Value::Type::kNull;
            return true;
        }
        return Fail("unexpected character");
    }

    bool ParseNumber(Value* out) {
        const std::size_t start = pos_;
        if (pos_ < in_.size() && in_[pos_] == '-') ++pos_;
        const std::size_t digits = pos_;
        while (pos_ < in_.size() && in_[pos_] >= '0' && in_[pos_] <= '9') ++pos_;
        if (digits == pos_) return Fail("expected a digit");
        // JSON numbers cannot have redundant leading zeros: 0 is fine, 01 is not.
        if (in_[digits] == '0' && pos_ - digits > 1) return Fail("leading zeros are not allowed");
        if (pos_ < in_.size() && (in_[pos_] == '.' || in_[pos_] == 'e' || in_[pos_] == 'E')) {
            if (!allow_float_) {
                return Fail("floating point numbers are not supported");
            }
            if (in_[pos_] == '.') {
                ++pos_;
                const std::size_t frac = pos_;
                while (pos_ < in_.size() && in_[pos_] >= '0' && in_[pos_] <= '9') ++pos_;
                if (frac == pos_) return Fail("expected a digit after decimal point");
            }
            if (pos_ < in_.size() && (in_[pos_] == 'e' || in_[pos_] == 'E')) {
                ++pos_;
                if (pos_ < in_.size() && (in_[pos_] == '+' || in_[pos_] == '-')) ++pos_;
                const std::size_t exp = pos_;
                while (pos_ < in_.size() && in_[pos_] >= '0' && in_[pos_] <= '9') ++pos_;
                if (exp == pos_) return Fail("expected a digit in exponent");
            }
            errno = 0;
            char* end = nullptr;
            const double parsed = std::strtod(in_.c_str() + start, &end);
            if (end != in_.c_str() + pos_ || !std::isfinite(parsed) ||
                parsed < -9223372036854775808.0 || parsed >= 9223372036854775808.0) {
                return Fail("number out of range");
            }
            out->type = Value::Type::kNumber;
            out->number = std::llround(parsed);
            return true;
        }
        errno = 0;
        const long long parsed = std::strtoll(in_.c_str() + start, nullptr, 10);
        if (errno != 0) return Fail("number out of range");
        out->type = Value::Type::kNumber;
        out->number = parsed;
        return true;
    }

    bool ParseString(std::string* out) {
        if (in_[pos_] != '"') return Fail("expected a string");
        ++pos_;
        out->clear();
        while (pos_ < in_.size()) {
            const char c = in_[pos_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos_ >= in_.size()) return Fail("unterminated escape");
                const char e = in_[pos_++];
                switch (e) {
                    case '"': out->push_back('"'); break;
                    case '\\': out->push_back('\\'); break;
                    case '/': out->push_back('/'); break;
                    case 'b': out->push_back('\b'); break;
                    case 'f': out->push_back('\f'); break;
                    case 'n': out->push_back('\n'); break;
                    case 'r': out->push_back('\r'); break;
                    case 't': out->push_back('\t'); break;
                    case 'u': {
                        unsigned code = 0;
                        if (!ReadHex4(&code)) return false;
                        if (code >= 0xD800 && code <= 0xDBFF) {
                            // A high surrogate must be followed by a low surrogate;
                            // emitting it alone would produce invalid UTF-8.
                            if (pos_ + 6 > in_.size() || in_[pos_] != '\\' ||
                                in_[pos_ + 1] != 'u') {
                                return Fail("lone high surrogate");
                            }
                            pos_ += 2;
                            unsigned low = 0;
                            if (!ReadHex4(&low)) return false;
                            if (low < 0xDC00 || low > 0xDFFF) return Fail("lone high surrogate");
                            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                        } else if (code >= 0xDC00 && code <= 0xDFFF) {
                            return Fail("lone low surrogate");
                        }
                        AppendUtf8(out, code);
                        break;
                    }
                    default: return Fail("unknown escape");
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return Fail("raw control character in string");
            } else {
                out->push_back(c);
            }
        }
        return Fail("unterminated string");
    }

    bool ReadHex4(unsigned* code) {
        if (pos_ + 4 > in_.size()) return Fail("truncated \\u escape");
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = in_[pos_++];
            unsigned digit = 0;
            if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
            else return Fail("bad \\u digit");
            value = value * 16 + digit;
        }
        *code = value;
        return true;
    }

    static void AppendUtf8(std::string* out, unsigned code) {
        if (code < 0x80) {
            out->push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            out->push_back(static_cast<char>(0xC0 | (code >> 6)));
            out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            out->push_back(static_cast<char>(0xE0 | (code >> 12)));
            out->push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out->push_back(static_cast<char>(0xF0 | (code >> 18)));
            out->push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    bool ParseArray(Value* out) {
        ++pos_;  // '['
        out->type = Value::Type::kArray;
        out->array.clear();
        SkipSpace();
        if (pos_ < in_.size() && in_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            out->array.emplace_back();
            if (!ParseValue(&out->array.back())) return false;
            SkipSpace();
            if (pos_ >= in_.size()) return Fail("unterminated array");
            if (in_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (in_[pos_] == ']') {
                ++pos_;
                return true;
            }
            return Fail("expected , or ] in array");
        }
    }

    bool ParseObject(Value* out) {
        ++pos_;  // '{'
        out->type = Value::Type::kObject;
        out->object.clear();
        SkipSpace();
        if (pos_ < in_.size() && in_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            SkipSpace();
            std::string key;
            if (!ParseString(&key)) return false;
            SkipSpace();
            if (pos_ >= in_.size() || in_[pos_] != ':') return Fail("expected : after key");
            ++pos_;
            out->object.emplace_back(std::move(key), Value{});
            if (!ParseValue(&out->object.back().second)) return false;
            SkipSpace();
            if (pos_ >= in_.size()) return Fail("unterminated object");
            if (in_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (in_[pos_] == '}') {
                ++pos_;
                return true;
            }
            return Fail("expected , or } in object");
        }
    }

    static constexpr int kMaxDepth = 32;
    const std::string& in_;
    bool allow_float_ = false;
    std::size_t pos_ = 0;
    int depth_ = 0;
    std::string error_;
};

}  // namespace

const Value* Value::Find(const char* key) const {
    for (const auto& entry : object) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

bool Value::GetString(const char* key, std::string* out) const {
    const Value* member = Find(key);
    if (member == nullptr || member->type != Type::kString) return false;
    *out = member->text;
    return true;
}

bool Value::GetInt(const char* key, long long* out) const {
    const Value* member = Find(key);
    if (member == nullptr || member->type != Type::kNumber) return false;
    *out = member->number;
    return true;
}

bool Value::GetBool(const char* key, bool* out) const {
    const Value* member = Find(key);
    if (member == nullptr || member->type != Type::kBool) return false;
    *out = member->boolean;
    return true;
}

bool Parse(const std::string& text, Value* out, std::string* error) {
    Parser parser(text, false);
    return parser.ParseDocument(out, error);
}

bool ParseAllowFloat(const std::string& text, Value* out, std::string* error) {
    Parser parser(text, true);
    return parser.ParseDocument(out, error);
}

std::string Escape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 2);
    for (const char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04X", static_cast<unsigned>(c));
                    out += buffer;
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    return out;
}

}  // namespace json
