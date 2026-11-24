#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "json.h"
#include "macro_json.h"
#include "macro_store.h"
#include "protocol.h"

namespace {

int g_failures = 0;

void Expect(bool condition, const char* what) {
    if (condition) return;
    ++g_failures;
    std::printf("FAIL: %s\n", what);
}

const char* kStudio = R"({
  "format": "incott-studio-macro",
  "version": 1,
  "macro": {
    "name": "double tap A",
    "looptype": 3,
    "cycle": 2,
    "uid": 1763740000000,
    "item": [
      { "key": "KeyA", "press": true, "delay": 30 },
      { "key": "KeyA", "press": false, "delay": 30 }
    ]
  }
})";

// Golden test vectors matching vendor macro_to_hw() byte-for-byte across the matrix.
const std::uint8_t kExpectedHw_Vector1[320] = {
    0x00, 0x02, 0x05, 0x00, 0x01, 0x04, 0x1e, 0x00, 0x80, 0x04, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x8c, 0x00, 0x00, 0x00, 0x10, 0x00, 0xe8, 0xe8, 0x00, 0x9f, 0x18, 0xa7, 0x04, 0x00, 0x02, 0x00,
};

const std::uint8_t kExpectedHw_Vector2[320] = {
    0x05, 0x00, 0x01, 0x00, 0x01, 0xf0, 0x0a, 0x00, 0x01, 0xf1, 0x14, 0x00, 0x81, 0xf1, 0x00, 0x00,
    0x80, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x94, 0x00, 0x00, 0x00, 0x10, 0x00, 0xe8, 0xe8, 0xff, 0xff, 0xff, 0xff, 0x08, 0x00, 0x04, 0x00,
};

const std::uint8_t kExpectedHw_Vector3[320] = {
    0x03, 0x01, 0xff, 0xff, 0x01, 0xf2, 0xff, 0xff, 0x80, 0xf2, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x8c, 0x00, 0x00, 0x00, 0x10, 0x00, 0xe8, 0xe8, 0xd2, 0x02, 0x96, 0x49, 0x04, 0x00, 0x02, 0x00,
};

const std::uint8_t kExpectedHw_Vector4[320] = {
    0x01, 0x02, 0x01, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00,
    0x81, 0x05, 0x05, 0x00, 0x01, 0x05, 0x05, 0x00, 0x81, 0x05, 0x05, 0x00, 0x80, 0x04, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xa0, 0x01, 0x00, 0x00, 0x10, 0x00, 0xe8, 0xe8, 0x2a, 0x00, 0x00, 0x00, 0x8e, 0x00, 0x47, 0x00,
};

}  // namespace

int main() {
    // ---- json reader ----
    {
        json::Value value;
        std::string error;
        Expect(json::Parse(R"({"a":1,"b":[true,null,"x\ny"],"c":"\u4e2d"})", &value, &error),
               "parse object");
        long long a = 0;
        Expect(value.GetInt("a", &a) && a == 1, "read int");
        const json::Value* b = value.Find("b");
        Expect(b != nullptr && b->type == json::Value::Type::kArray && b->array.size() == 3,
               "read array");
        Expect(b->array[1].type == json::Value::Type::kNull, "read null");
        std::string c;
        Expect(value.GetString("c", &c) && c == "\xE4\xB8\xAD", "unicode escape");
        Expect(!json::Parse("{", &value, &error), "reject truncated object");
        Expect(!json::Parse("{\"a\":1.5}", &value, &error), "reject float");
        Expect(json::Escape("a\"b\\c") == "a\\\"b\\\\c", "escape quotes");

        // UTF-8 BOM detection & skipping
        const std::string withBom = "\xEF\xBB\xBF{\"bom\":true}";
        Expect(json::Parse(withBom, &value, &error), "parse json with utf-8 bom");
        bool bomVal = false;
        Expect(value.GetBool("bom", &bomVal) && bomVal, "bom value parsed correctly");

        // Recursion depth limit kMaxDepth = 32
        std::string deep;
        for (int i = 0; i < 33; ++i) deep += "[";
        for (int i = 0; i < 33; ++i) deep += "]";
        Expect(!json::Parse(deep, &value, &error), "reject recursion depth 33");
        Expect(error.find("nesting depth exceeded limit") != std::string::npos,
               "recursion depth error text");

        std::string validDeep;
        for (int i = 0; i < 30; ++i) validDeep += "[";
        validDeep += "1";
        for (int i = 0; i < 30; ++i) validDeep += "]";
        Expect(json::Parse(validDeep, &value, &error), "accept recursion depth <= 32");
    }

    // ---- Studio JSON import ----
    {
        incott::Macro macro;
        std::string error;
        Expect(incott::ParseMacroJson(kStudio, &macro, &error), "import studio macro");
        Expect(macro.name == L"double tap A", "studio name");
        Expect(macro.loopType == 3 && macro.cycles == 2, "studio loop/cycle");
        Expect(macro.uid == 1763740000000ULL, "uid preserved as 64-bit safe integer");
        Expect(macro.actions.size() == 2, "studio item count");
        Expect(macro.actions[0].press && macro.actions[0].usage == 0x04 &&
                   macro.actions[0].delayMs == 30,
               "studio first item");
        Expect(!macro.actions[1].press, "studio second item release");

        incott::Macro copy;
        Expect(incott::ParseMacroJson(incott::MacroToJson(macro), &copy, &error),
               "re-import exported json");
        Expect(copy.name == macro.name && copy.loopType == macro.loopType &&
                   copy.cycles == macro.cycles && copy.uid == macro.uid &&
                   copy.actions.size() == macro.actions.size(),
               "export roundtrip");

        Expect(incott::MacroSummaryJson(macro) ==
                   "{\"name\":\"double tap A\",\"loop\":3,\"cycles\":2,\"actions\":2}",
               "summary json");
    }

    // ---- 64-bit UID roundtrip ----
    {
        incott::Macro macro;
        std::string error;
        const std::string maxUidJson = R"({
          "format": "incott-studio-macro",
          "version": 1,
          "macro": {
            "name": "max uid macro",
            "looptype": 1,
            "cycle": 1,
            "uid": 9007199254740991,
            "item": [
              { "key": "KeyA", "press": true, "delay": 10 },
              { "key": "KeyA", "press": false, "delay": 10 }
            ]
          }
        })";
        Expect(incott::ParseMacroJson(maxUidJson, &macro, &error), "parse max safe uid (2^53 - 1)");
        Expect(macro.uid == 9007199254740991ULL, "uid matches 9007199254740991");
        const std::string exported = incott::MacroToJson(macro);
        Expect(exported.find("9007199254740991") != std::string::npos,
               "MacroToJson preserves 64-bit uid");
        incott::Macro reloaded;
        Expect(incott::ParseMacroJson(exported, &reloaded, &error), "re-import max safe uid");
        Expect(reloaded.uid == 9007199254740991ULL, "reloaded uid preserved");
    }

    // ---- Studio JSON envelope and format validation ----
    {
        incott::Macro macro;
        std::string error;
        const std::string badFormat = R"({"format":"other-format","version":1,"macro":{"name":"x","looptype":1,"cycle":1,"item":[{"key":"KeyA","press":true,"delay":0},{"key":"KeyA","press":false,"delay":0}]}})";
        Expect(!incott::ParseMacroJson(badFormat, &macro, &error), "reject invalid format");
        Expect(error.find("unsupported macro format") != std::string::npos, "format error text");

        const std::string badVersion = R"({"format":"incott-studio-macro","version":2,"macro":{"name":"x","looptype":1,"cycle":1,"item":[{"key":"KeyA","press":true,"delay":0},{"key":"KeyA","press":false,"delay":0}]}})";
        Expect(!incott::ParseMacroJson(badVersion, &macro, &error), "reject invalid version 2");
        Expect(error.find("unsupported format version") != std::string::npos, "version error text");

        // Bare object fallback
        const std::string bareObj = R"({"name":"bare","looptype":1,"cycle":1,"item":[{"key":"KeyA","press":true,"delay":0},{"key":"KeyA","press":false,"delay":0}]})";
        Expect(incott::ParseMacroJson(bareObj, &macro, &error), "accept bare object format");
        Expect(macro.name == L"bare", "bare object name");

        // 128KB payload size rejection
        std::string hugeJson(131073, ' ');
        Expect(!incott::ParseMacroJson(hugeJson, &macro, &error), "reject payload > 128 KB");
        Expect(error.find("128 KB") != std::string::npos, "128 KB error text");
    }

    // ---- Studio JSON validation & held state machine ----
    {
        incott::Macro macro;
        std::string error;
        const std::string badKey = R"({"macro":{"name":"x","looptype":1,"cycle":1,
            "item":[{"key":"KeyA","press":true,"delay":0},{"key":"KeyZz","press":true,"delay":0}]}})";
        Expect(!incott::ParseMacroJson(badKey, &macro, &error), "reject unknown key");
        Expect(error.find("item 1: unknown key: KeyZz") != std::string::npos,
               "unknown key error names the item and key");

        const std::string dupPress = R"({"macro":{"name":"x","looptype":1,"cycle":1,
            "item":[{"key":"KeyA","press":true,"delay":10},{"key":"KeyA","press":true,"delay":10}]}})";
        Expect(!incott::ParseMacroJson(dupPress, &macro, &error), "reject duplicate press");
        Expect(error.find("item 1: duplicate press for key: KeyA") != std::string::npos,
               "duplicate press error text");

        const std::string orphanRelease = R"({"macro":{"name":"x","looptype":1,"cycle":1,
            "item":[{"key":"KeyB","press":false,"delay":10}]}})";
        Expect(!incott::ParseMacroJson(orphanRelease, &macro, &error), "reject release without press");
        Expect(error.find("item 0: release without press for key: KeyB") != std::string::npos,
               "release without press error text");

        const std::string tooHigh = R"({"macro":{"name":"x","looptype":1,"cycle":65536,
            "item":[{"key":"KeyA","press":true,"delay":0}]}})";
        Expect(!incott::ParseMacroJson(tooHigh, &macro, &error), "reject cycle 65536");
        Expect(error.find("cycles must be 1..65535") != std::string::npos, "cycle error text");

        const std::string zeroCycle = R"({"macro":{"name":"x","looptype":1,"cycle":0,
            "item":[{"key":"KeyA","press":true,"delay":0}]}})";
        Expect(!incott::ParseMacroJson(zeroCycle, &macro, &error), "reject cycle 0");

        std::string name33(33, 'n');
        const std::string longName = "{\"macro\":{\"name\":\"" + name33 +
            "\",\"looptype\":1,\"cycle\":1,\"item\":[{\"key\":\"KeyA\",\"press\":true,\"delay\":0}]}}";
        Expect(!incott::ParseMacroJson(longName, &macro, &error), "reject 33-char name");

        const std::string ctrlName = "{\"macro\":{\"name\":\"bad\\nname\",\"looptype\":1,\"cycle\":1,\"item\":[{\"key\":\"KeyA\",\"press\":true,\"delay\":0}]}}";
        Expect(!incott::ParseMacroJson(ctrlName, &macro, &error), "reject newline in name");

        std::string items;
        for (int i = 0; i < 72; ++i) {
            if (i > 0) items += ",";
            items += R"({"key":"KeyA","press":true,"delay":0})";
        }
        const std::string tooMany = "{\"macro\":{\"name\":\"x\",\"looptype\":1,\"cycle\":1,\"item\":[" +
                                    items + "]}}";
        Expect(!incott::ParseMacroJson(tooMany, &macro, &error), "reject 72 items");
        Expect(error.find("more than 71") != std::string::npos, "72-item error text");
    }

    // ---- LIFO dangling press auto-close on save ----
    {
        incott::Macro macro;
        std::string error;
        // 3 dangling presses: A down, B down, C down -> must close in LIFO: C up, B up, A up
        const std::string dangling3 = R"({"macro":{"name":"abc","looptype":3,"cycle":1,
            "item":[{"key":"KeyA","press":true,"delay":10},
                    {"key":"KeyB","press":true,"delay":20},
                    {"key":"KeyC","press":true,"delay":30}]}})";
        Expect(incott::ParseMacroJson(dangling3, &macro, &error), "parse 3-key dangling macro");
        Expect(incott::CloseMacroTail(&macro, &error), "close 3 dangling presses");
        Expect(macro.actions.size() == 6, "6 actions after LIFO close");
        Expect(!macro.actions[3].press && macro.actions[3].usage == 0x06 && macro.actions[3].delayMs == 0,
               "LIFO close item 3 is KeyC release");
        Expect(!macro.actions[4].press && macro.actions[4].usage == 0x05 && macro.actions[4].delayMs == 0,
               "LIFO close item 4 is KeyB release");
        Expect(!macro.actions[5].press && macro.actions[5].usage == 0x04 && macro.actions[5].delayMs == 0,
               "LIFO close item 5 is KeyA release");

        // Single dangling press with preceding matched pair
        const std::string dangling1 = R"({"macro":{"name":"abc","looptype":3,"cycle":1,
            "item":[{"key":"KeyA","press":true,"delay":30},{"key":"KeyA","press":false,"delay":30},
                    {"key":"KeyB","press":true,"delay":0}]}})";
        Expect(incott::ParseMacroJson(dangling1, &macro, &error), "parse single dangling macro");
        Expect(incott::CloseMacroTail(&macro, &error), "close single dangling press");
        Expect(macro.actions.size() == 4 && !macro.actions.back().press &&
                   macro.actions.back().usage == 0x05,
               "release appended for the last key");

        // 70 items with 2 unclosed presses -> would become 72 items -> reject
        incott::Macro overLimit;
        for (int i = 0; i < 34; ++i) {
            overLimit.actions.push_back({true, 0x04, 5});
            overLimit.actions.push_back({false, 0x04, 5});
        }
        overLimit.actions.push_back({true, 0x04, 5}); // 69
        overLimit.actions.push_back({true, 0x05, 5}); // 70
        Expect(!incott::CloseMacroTail(&overLimit, &error), "reject 70 items + 2 releases > 71");
        Expect(error.find("71-action limit") != std::string::npos, "limit error text");

        incott::Macro full;
        for (int i = 0; i < incott::kMacroActionLimit; ++i) full.actions.push_back({true, 0x04, 10});
        Expect(!incott::CloseMacroTail(&full, &error), "71 presses leave no room for a release");
        Expect(error.find("71-action limit") != std::string::npos, "limit error text");

        incott::Macro endsReleased;
        for (int i = 0; i < 35; ++i) {
            endsReleased.actions.push_back({true, 0x04, 10});
            endsReleased.actions.push_back({false, 0x04, 10});
        }
        Expect(incott::CloseMacroTail(&endsReleased, &error), "70 items already ending in release");
    }

    // ---- 4 Golden test vectors matching vendor macro_to_hw byte-for-byte ----
    {
        // Vector 1: slot 0, looptype 3 (cycles 5), KeyA down (30ms), KeyA up (30ms), uid 1763740000000
        {
            const char* v1Json = R"({
              "format": "incott-studio-macro",
              "version": 1,
              "macro": {
                "name": "tap A",
                "looptype": 3,
                "cycle": 5,
                "uid": 1763740000000,
                "item": [
                  { "key": "KeyA", "press": true, "delay": 30 },
                  { "key": "KeyA", "press": false, "delay": 30 }
                ]
              }
            })";
            incott::Macro macro;
            std::string error;
            Expect(incott::ParseMacroJson(v1Json, &macro, &error), "parse golden vector 1");
            std::uint8_t buffer[incott::kMacroPayload] = {};
            const incott::MacroError rc =
                incott::EncodeMacro(0, macro.loopType, macro.cycles, macro.uid, macro.actions, buffer);
            Expect(rc == incott::MacroError::kOk, "encode golden vector 1");
            Expect(std::memcmp(buffer, kExpectedHw_Vector1, incott::kMacroPayload) == 0,
                   "golden vector 1 matches vendor macro_to_hw byte-for-byte");
        }

        // Vector 2: slot 5, looptype 1, cycle 1, LButton (0xF0), RButton (0xF1), uid 9007199254740991
        {
            const char* v2Json = R"({
              "format": "incott-studio-macro",
              "version": 1,
              "macro": {
                "name": "mouse lr",
                "looptype": 1,
                "cycle": 1,
                "uid": 9007199254740991,
                "item": [
                  { "key": "LButton", "press": true, "delay": 10 },
                  { "key": "RButton", "press": true, "delay": 20 },
                  { "key": "RButton", "press": false, "delay": 0 },
                  { "key": "LButton", "press": false, "delay": 0 }
                ]
              }
            })";
            incott::Macro macro;
            std::string error;
            Expect(incott::ParseMacroJson(v2Json, &macro, &error), "parse golden vector 2");
            std::uint8_t buffer[incott::kMacroPayload] = {};
            const incott::MacroError rc =
                incott::EncodeMacro(5, macro.loopType, macro.cycles, macro.uid, macro.actions, buffer);
            Expect(rc == incott::MacroError::kOk, "encode golden vector 2");
            Expect(std::memcmp(buffer, kExpectedHw_Vector2, incott::kMacroPayload) == 0,
                   "golden vector 2 matches vendor macro_to_hw byte-for-byte");
        }

        // Vector 3: slot 3, looptype 2, cycle 65535, MButton (0xF2), delay 65535, uid 1234567890
        {
            const char* v3Json = R"({
              "format": "incott-studio-macro",
              "version": 1,
              "macro": {
                "name": "mbutton max",
                "looptype": 2,
                "cycle": 65535,
                "uid": 1234567890,
                "item": [
                  { "key": "MButton", "press": true, "delay": 65535 },
                  { "key": "MButton", "press": false, "delay": 65535 }
                ]
              }
            })";
            incott::Macro macro;
            std::string error;
            Expect(incott::ParseMacroJson(v3Json, &macro, &error), "parse golden vector 3");
            std::uint8_t buffer[incott::kMacroPayload] = {};
            const incott::MacroError rc =
                incott::EncodeMacro(3, macro.loopType, macro.cycles, macro.uid, macro.actions, buffer);
            Expect(rc == incott::MacroError::kOk, "encode golden vector 3");
            Expect(std::memcmp(buffer, kExpectedHw_Vector3, incott::kMacroPayload) == 0,
                   "golden vector 3 matches vendor macro_to_hw byte-for-byte");
        }

        // Vector 4: slot 1, looptype 3, cycle 1, 71 items (35 pairs of KeyB + 1 KeyA up), uid 42
        {
            incott::Macro macro;
            macro.name = L"max 71 items";
            macro.loopType = 3;
            macro.cycles = 1;
            macro.uid = 42;
            for (int i = 0; i < 35; ++i) {
                macro.actions.push_back({true, 0x05, 5});
                macro.actions.push_back({false, 0x05, 5});
            }
            macro.actions.push_back({false, 0x04, 0}); // 71st item
            std::uint8_t buffer[incott::kMacroPayload] = {};
            const incott::MacroError rc =
                incott::EncodeMacro(1, macro.loopType, macro.cycles, macro.uid, macro.actions, buffer);
            Expect(rc == incott::MacroError::kOk, "encode golden vector 4");
            Expect(std::memcmp(buffer, kExpectedHw_Vector4, incott::kMacroPayload) == 0,
                   "golden vector 4 matches vendor macro_to_hw byte-for-byte");
        }
    }

    // ---- local library text form ----
    {
        incott::MacroStore store;
        incott::Macro macro;
        std::string error;
        Expect(incott::ParseMacroJson(kStudio, &macro, &error), "library fixture");
        store.At(6).macro = macro;
        store.At(6).defined = true;
        store.SetBinding(4, 6);

        const std::wstring text = store.Serialize();
        incott::MacroStore loaded;
        std::wstring wideError;
        Expect(loaded.Deserialize(text, &wideError), "reload library");
        Expect(loaded.At(6).defined && loaded.At(6).macro.name == macro.name, "reload slot 6");
        Expect(loaded.At(6).dirty, "loaded slot is dirty (must be re-uploaded)");
        Expect(loaded.Binding(4) == 6 && loaded.Binding(0) == -1, "reload binding");
        Expect(loaded.At(6).macro.uid == 1763740000000ULL, "reload preserves 64-bit uid");
        Expect(loaded.Serialize() == text, "library text roundtrip");

        incott::MacroStore empty;
        Expect(!empty.Deserialize(L"nope\n", &wideError), "reject missing header");
        Expect(!empty.Deserialize(L"periphhub-macros 1\nslot 10\n", &wideError), "reject slot 10");
        Expect(!empty.Deserialize(
                   L"periphhub-macros 1\nslot 0\nloop 1\ncycles 1\naction 1 4 0\nbind 6 0\n",
                   &wideError),
               "reject key 6 binding");
    }

    // ---- import closes dangling presses; library and JSON hardening ----
    {
        // The import path itself closes a trailing press (zero-delay release in
        // LIFO order) and succeeds instead of silently dropping the key.
        const std::string trailingPress = R"({
  "format": "incott-studio-macro",
  "version": 1,
  "macro": {
    "name": "dangling",
    "looptype": 1,
    "cycle": 1,
    "item": [
      { "key": "KeyA", "press": true, "delay": 10 },
      { "key": "KeyB", "press": true, "delay": 10 }
    ]
  }
})";
        incott::Macro macro;
        std::string error;
        Expect(incott::ParseMacroJson(trailingPress, &macro, &error),
               "import ending in a press succeeds");
        Expect(macro.actions.size() == 4, "import appends one release per held key");
        if (macro.actions.size() == 4) {
            Expect(!macro.actions[2].press && macro.actions[2].usage == 0x05 &&
                       macro.actions[2].delayMs == 0,
                   "import releases KeyB first (LIFO)");
            Expect(!macro.actions[3].press && macro.actions[3].usage == 0x04 &&
                       macro.actions[3].delayMs == 0,
                   "import releases KeyA last (LIFO)");
        }

        // A balanced macro is untouched by the automatic close.
        const std::string balanced = R"({
  "format": "incott-studio-macro",
  "version": 1,
  "macro": {
    "name": "balanced",
    "looptype": 1,
    "cycle": 1,
    "item": [
      { "key": "KeyA", "press": true, "delay": 10 },
      { "key": "KeyA", "press": false, "delay": 10 }
    ]
  }
})";
        incott::Macro balancedMacro;
        Expect(incott::ParseMacroJson(balanced, &balancedMacro, &error),
               "balanced import succeeds");
        Expect(balancedMacro.actions.size() == 2, "balanced import keeps two actions");

        // A release without a press is still rejected.
        const std::string strayRelease = R"({
  "format": "incott-studio-macro",
  "version": 1,
  "macro": {
    "name": "stray",
    "looptype": 1,
    "cycle": 1,
    "item": [ { "key": "KeyA", "press": false, "delay": 1 } ]
  }
})";
        incott::Macro strayMacro;
        Expect(!incott::ParseMacroJson(strayRelease, &strayMacro, &error),
               "import rejects a release without a press");

        // Closing the tail must not push the macro past the 71-action limit.
        std::string items;
        for (int i = 0; i < 35; ++i) {
            items += R"({ "key": "KeyB", "press": true, "delay": 5 },)";
            items += R"({ "key": "KeyB", "press": false, "delay": 5 },)";
        }
        items += R"({ "key": "KeyA", "press": true, "delay": 0 })";
        const std::string overLimit =
            R"({"format":"incott-studio-macro","version":1,"macro":{"name":"limit","looptype":1,"cycle":1,"item":[)" +
            items + "]}}";
        incott::Macro limitMacro;
        Expect(!incott::ParseMacroJson(overLimit, &limitMacro, &error),
               "import rejects 71 raw items that close to 72 actions");
        Expect(error.find("no room for its release") != std::string::npos,
               "over-limit close uses the documented error");
    }

    // ---- Deserialize is transactional and numbers lines from 1 ----
    {
        incott::MacroStore store;
        store.At(3).macro.name = L"keep me";
        store.At(3).macro.loopType = 1;
        store.At(3).macro.cycles = 1;
        store.At(3).macro.actions.push_back({true, 0x04, 5});
        store.At(3).defined = true;
        store.SetBinding(1, 3);

        const auto unchanged = [&store]() {
            return store.At(3).defined && store.At(3).macro.name == L"keep me" &&
                   store.Binding(1) == 3;
        };
        std::wstring wideError;

        Expect(!store.Deserialize(L"garbage\n", &wideError), "reject garbage library file");
        Expect(unchanged(), "garbage does not clobber the library");
        Expect(!store.At(0).defined, "failed load leaves slot 0 undefined");
        Expect(wideError.find(L"line 1") != std::wstring::npos,
               "header error names line 1");

        Expect(!store.Deserialize(L"periphhub-macros 1\nslot nope\n", &wideError),
               "reject non-numeric slot index");
        Expect(wideError.find(L"line 2") != std::wstring::npos,
               "first bad body line is reported as line 2 (1-based)");

        Expect(!store.Deserialize(L"periphhub-macros 1\nslot 0\nbogus 1\n", &wideError),
               "reject unknown keyword");
        Expect(unchanged(), "unknown keyword does not clobber the library");
        Expect(wideError.find(L"line 3") != std::wstring::npos,
               "unknown keyword names line 3");

        Expect(!store.Deserialize(L"periphhub-macros 1\nslot 0\nloop nope\n", &wideError),
               "reject non-numeric loop type");
        Expect(unchanged(), "non-numeric loop type does not clobber the library");

        Expect(!store.Deserialize(L"periphhub-macros 1\nslot 0\nloop 1 2\n", &wideError),
               "reject trailing operand on loop line");

        Expect(!store.Deserialize(L"periphhub-macros 1\nslot 0\ncycles 0\n", &wideError),
               "reject out-of-range cycles");

        Expect(!store.Deserialize(L"periphhub-macros 1\nslot 0\naction 2 4 0\n", &wideError),
               "reject out-of-range action press");
        Expect(unchanged(), "invalid action does not clobber the library");
        Expect(!store.Deserialize(L"periphhub-macros 1\nslot 0\naction 1 4\n", &wideError),
               "reject missing action operand");

        incott::MacroStore reloaded;
        Expect(reloaded.Deserialize(store.Serialize(), &wideError),
               "valid library text still reloads");
        Expect(reloaded.At(3).defined && reloaded.Binding(1) == 3,
               "reloaded library keeps slot and binding");
    }

    // ---- JSON reader hardening: leading zeros and lone surrogates ----
    {
        json::Value value;
        std::string error;
        Expect(!json::Parse("01", &value, &error), "reject leading zero");
        Expect(!json::Parse("[01]", &value, &error), "reject leading zero inside array");
        Expect(json::Parse("0", &value, &error), "accept plain zero");
        Expect(json::Parse(R"("\ud83d\ude00")", &value, &error), "accept surrogate pair");
        Expect(!json::Parse(R"("\ud800")", &value, &error), "reject lone high surrogate");
        Expect(!json::Parse(R"("\udc00")", &value, &error), "reject lone low surrogate");
        Expect(!json::Parse(R"("\ud800x")", &value, &error),
               "reject high surrogate followed by text");
        Expect(!json::Parse(R"("\ud800\u0041")", &value, &error),
               "reject high surrogate followed by a non-low escape");
    }

    // ---- WP1-5: legacy web-UI export (incott-g23-macro-v1) is auto-converted ----
    {
        // The first web workbench exported {schema, name, loopMode, loopCount,
        // actions:[{type,key,ms}]} with Chinese key labels; the parser must map it
        // onto the Studio schema instead of rejecting it.
        const char* legacy = R"({
          "schema": "incott-g23-macro-v1",
          "name": "旧版连招",
          "loopMode": "count",
          "loopCount": 2,
          "maxActions": 71,
          "actions": [
            { "type": "down", "key": "鼠标左键" },
            { "type": "delay", "ms": 28 },
            { "type": "up", "key": "鼠标左键" },
            { "type": "delay", "ms": 35 },
            { "type": "down", "key": "Q" },
            { "type": "delay", "ms": 20 },
            { "type": "up", "key": "Q" }
          ]
        })";
        incott::Macro macro;
        std::string error;
        Expect(incott::ParseMacroJson(legacy, &macro, &error), "import legacy schema macro");
        Expect(error.empty(), "legacy import has no residual error");
        Expect(macro.name == L"旧版连招", "legacy import keeps the name");
        Expect(macro.loopType == 3, "legacy count maps to looptype 3");
        Expect(macro.cycles == 2, "legacy loopCount maps to cycles");
        // Standalone delay rows merge into the previous key event (the vendor
        // editor stores the hold duration on the press item).
        Expect(macro.actions.size() == 4, "legacy delays collapse into 4 items");
        Expect(macro.actions[0].press && macro.actions[0].usage == 0xF0 &&
                   macro.actions[0].delayMs == 28,
               "鼠标左键 down carries the 28ms hold delay");
        Expect(!macro.actions[1].press && macro.actions[1].usage == 0xF0 &&
                   macro.actions[1].delayMs == 35,
               "鼠标左键 up carries the 35ms gap");
        Expect(macro.actions[2].press && macro.actions[2].usage == 0x14 &&
                   macro.actions[2].delayMs == 20,
               "bare Q maps to KeyQ usage 0x14 with its 20ms delay");
        Expect(!macro.actions[3].press && macro.actions[3].usage == 0x14 &&
                   macro.actions[3].delayMs == 0,
               "trailing release has no delay");
        std::uint8_t buffer[incott::kMacroPayload] = {};
        Expect(incott::EncodeMacro(0, macro.loopType, macro.cycles, macro.uid, macro.actions,
                                   buffer) == incott::MacroError::kOk,
               "converted legacy macro encodes");

        // Schema-less pastes of the old editor shape import too.
        const char* legacyBare =
            R"({"name":"粘贴","loopMode":"hold","actions":[{"type":"down","key":"Ctrl"},)"
            R"({"type":"up","key":"Ctrl"}]})";
        incott::Macro bare;
        Expect(incott::ParseMacroJson(legacyBare, &bare, &error), "import schema-less legacy paste");
        Expect(bare.loopType == 1 && bare.actions.size() == 2 &&
                   bare.actions[0].usage == 0xE0 && bare.actions[0].press &&
                   bare.actions[1].usage == 0xE0 && !bare.actions[1].press,
               "hold maps to looptype 1 and Ctrl to ControlLeft 0xE0 press/release");

        const char* legacyToggle =
            R"({"name":"t","loopMode":"toggle","loopCount":9,"actions":[{"type":"down","key":"5"},)"
            R"({"type":"delay","ms":10},{"type":"up","key":"5"}]})";
        incott::Macro toggle;
        Expect(incott::ParseMacroJson(legacyToggle, &toggle, &error), "import toggle legacy");
        Expect(toggle.loopType == 2 && toggle.actions[0].usage == 0x22,
               "toggle maps to looptype 2 and bare 5 to Digit5 0x22");

        // Leading delays shift onto the first event; trailing onto the last.
        const char* legacyEdges =
            R"({"name":"edges","loopMode":"count","actions":[{"type":"delay","ms":100},)"
            R"({"type":"down","key":"A"},{"type":"up","key":"A"},{"type":"delay","ms":200}]})";
        incott::Macro edges;
        Expect(incott::ParseMacroJson(legacyEdges, &edges, &error), "import leading/trailing delays");
        Expect(edges.actions.size() == 2 && edges.actions[0].delayMs == 100 &&
                   edges.actions[1].delayMs == 200,
               "leading delay shifts to the press, trailing to the release");

        // Keys without a macro encoding fail with the usual unknown-key error.
        const char* legacySideKey =
            R"({"schema":"incott-g23-macro-v1","name":"bad","loopMode":"count",)"
            R"("actions":[{"type":"down","key":"前侧键"}]})";
        incott::Macro bad;
        Expect(!incott::ParseMacroJson(legacySideKey, &bad, &error),
               "reject 前侧键 (no 0xF3 encoding in the macro keytable)");
        Expect(error.find("unknown key") != std::string::npos, "side-key error names the key");

        const char* legacyBadType =
            R"({"schema":"incott-g23-macro-v1","name":"bad","loopMode":"count",)"
            R"("actions":[{"type":"click","key":"Q"}]})";
        Expect(!incott::ParseMacroJson(legacyBadType, &bad, &error),
               "reject unknown legacy action type");
        Expect(error.find("unknown type") != std::string::npos, "type error is specific");

        // 72 legacy rows collapse to 71 items -> accepted; 72 key events stay rejected.
        {
            std::string rows;
            for (int i = 0; i < 71; ++i) {
                if (!rows.empty()) rows += ",";
                rows += "{\"type\":\"down\",\"key\":\"Q\"},{\"type\":\"up\",\"key\":\"Q\"}";
            }
            const std::string pairy =
                R"({"schema":"incott-g23-macro-v1","name":"pairs","loopMode":"count","actions":[)" +
                rows + "]}";
            incott::Macro pairs;
            Expect(!incott::ParseMacroJson(pairy, &pairs, &error),
                   "reject legacy file whose 71 pairs stay 142 items");
            Expect(error.find("71") != std::string::npos, "71-item limit still enforced");

            std::string merged;
            for (int i = 0; i < 35; ++i) {
                if (!merged.empty()) merged += ",";
                merged += "{\"type\":\"down\",\"key\":\"Q\"},{\"type\":\"delay\",\"ms\":5},"
                          "{\"type\":\"up\",\"key\":\"Q\"},{\"type\":\"delay\",\"ms\":5}";
            }
            const std::string allMerged =
                R"({"schema":"incott-g23-macro-v1","name":"merged","loopMode":"count","actions":[)" +
                merged + "]}";
            incott::Macro mergedMacro;
            Expect(incott::ParseMacroJson(allMerged, &mergedMacro, &error),
                   "140 legacy rows collapse to 70 balanced items");
            Expect(mergedMacro.actions.size() == 70, "delays merged onto key events");
        }
    }

    // ---- F1-6: clearing a key binding in MacroStore persists across round-trip ----
    {
        incott::MacroStore store;
        store.At(2).defined = true;
        store.At(2).macro.name = L"Combo";
        store.At(2).macro.actions = {{true, 0x04, 10}, {false, 0x04, 0}};
        store.SetBinding(4, 2);
        Expect(store.Binding(4) == 2, "binding set to macro slot 2");

        // Simulates keys.set rebinding button 4 to Key / Fire / Off.
        store.SetBinding(4, -1);
        Expect(store.Binding(4) == -1, "binding cleared in store");

        incott::MacroStore reloaded;
        std::wstring err;
        Expect(reloaded.Deserialize(store.Serialize(), &err), "cleared binding roundtrips");
        Expect(reloaded.Binding(4) == -1, "cleared binding stays -1 after deserialize");
    }

    if (g_failures == 0) {
        std::printf("macro_test: OK (json, studio import/export, library persistence)\n");
        return 0;
    }
    std::printf("macro_test: %d failure(s)\n", g_failures);
    return 1;
}
