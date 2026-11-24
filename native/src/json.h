#pragma once
// Minimal JSON reader (objects, arrays, strings with \u escapes, integers).
// Only what the macro files need: no floating point, no duplicate-key policy.
#include <string>
#include <utility>
#include <vector>

namespace json {

struct Value {
    enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

    Type type = Type::kNull;
    bool boolean = false;
    long long number = 0;
    std::string text;
    std::vector<Value> array;
    std::vector<std::pair<std::string, Value>> object;

    // First member with this name, or nullptr.
    const Value* Find(const char* key) const;
    // Typed accessors: return false when the member is missing / wrong type.
    bool GetString(const char* key, std::string* out) const;
    bool GetInt(const char* key, long long* out) const;
    bool GetBool(const char* key, bool* out) const;
};

// Parses exactly one value with only whitespace after it.
bool Parse(const std::string& text, Value* out, std::string* error);

// Parses one value while accepting JSON floating-point numbers (rounded to integer).
bool ParseAllowFloat(const std::string& text, Value* out, std::string* error);

// Escapes a string for JSON output (does not add the surrounding quotes).
std::string Escape(const std::string& text);

}  // namespace json
