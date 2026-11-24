#include "macro_json.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "json.h"

namespace incott {

namespace {

std::string WidenToUtf8(const std::wstring& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned w = static_cast<unsigned short>(text[i]);
        if (w >= 0xD800 && w <= 0xDBFF && i + 1 < text.size()) {
            const unsigned low = static_cast<unsigned short>(text[i + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                w = 0x10000 + ((w - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        if (w < 0x80) {
            out.push_back(static_cast<char>(w));
        } else if (w < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (w >> 6)));
            out.push_back(static_cast<char>(0x80 | (w & 0x3F)));
        } else if (w < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (w >> 12)));
            out.push_back(static_cast<char>(0x80 | ((w >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (w & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (w >> 18)));
            out.push_back(static_cast<char>(0x80 | ((w >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((w >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (w & 0x3F)));
        }
    }
    return out;
}

// Minimal UTF-8 -> UTF-16 for names; malformed bytes become U+FFFD.
std::wstring Utf8ToWiden(const std::string& text) {
    std::wstring out;
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        unsigned code = 0xFFFD;
        std::size_t extra = 0;
        if (c < 0x80) {
            code = c;
        } else if ((c & 0xE0) == 0xC0) {
            code = c & 0x1F;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            code = c & 0x0F;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            code = c & 0x07;
            extra = 3;
        }
        ++i;
        bool ok = true;
        for (std::size_t k = 0; k < extra; ++k) {
            if (i >= text.size() || (static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            code = (code << 6) | (static_cast<unsigned char>(text[i]) & 0x3F);
            ++i;
        }
        if (!ok) {
            out.push_back(0xFFFD);
            continue;
        }
        if (code >= 0x10000) {
            code -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (code >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (code & 0x3FF)));
        } else {
            out.push_back(static_cast<wchar_t>(code));
        }
    }
    return out;
}

const char* KeyCodeForUsage(int usage) {
    const char* code = KeyCodeFromUsage(usage);
    return code != nullptr ? code : "Unknown";
}

// Old web-UI export format (docs/07 WP1-5): the first web workbench emitted
// `incott-g23-macro-v1` with UI labels (loopMode/type/ms, Chinese key names)
// instead of the Studio schema. Detection covers both the `schema`-tagged
// exports and schema-less `{name, loopMode, actions}` pastes.
bool IsLegacyMacroObject(const json::Value& root) {
    if (root.type != json::Value::Type::kObject) return false;
    std::string schema;
    if (root.GetString("schema", &schema)) return schema == "incott-g23-macro-v1";
    if (root.Find("format") != nullptr || root.Find("version") != nullptr ||
        root.Find("macro") != nullptr || root.Find("item") != nullptr) {
        return false;
    }
    const json::Value* actions = root.Find("actions");
    return actions != nullptr && actions->type == json::Value::Type::kArray;
}

// Old-UI key labels -> KeyboardEvent.code. Keys the macro keytable has no
// encoding for (e.g. 前侧键/后侧键: only LButton/RButton/MButton exist at
// 0xF0..0xF2) pass through and fail later with "unknown key: <name>".
std::string LegacyKeyToStudio(const std::string& key) {
    static const std::pair<const char*, const char*> kMap[] = {
        {"鼠标左键", "LButton"}, {"鼠标右键", "RButton"}, {"滚轮中键", "MButton"},
        {"Ctrl", "ControlLeft"}, {"Shift", "ShiftLeft"}, {"Alt", "AltLeft"},
        {"Win", "MetaLeft"}, {"Esc", "Escape"},
        {"Up", "ArrowUp"}, {"Down", "ArrowDown"}, {"Left", "ArrowLeft"},
        {"Right", "ArrowRight"},
    };
    for (const auto& entry : kMap) {
        if (key == entry.first) return entry.second;
    }
    // Old timeline used bare letters and digits (Q, 5).
    const unsigned char first = static_cast<unsigned char>(key.empty() ? 0 : key[0]);
    if (key.size() == 1 && first >= 'A' && first <= 'Z') return "Key" + key;
    if (key.size() == 1 && first >= '0' && first <= '9') return "Digit" + key;
    return key;  // already a code (F1, Space, Enter, ...) or unknown
}

int LegacyLoopType(const std::string& loopMode) {
    if (loopMode == "hold") return 1;    // 按住时循环 (lpUntilKeyRelease -> hw 0)
    if (loopMode == "toggle") return 2;  // 再次按下停止 (lpUntilAnyKey -> hw 1)
    return 3;                            // count (default in the old UI)
}

// The vendor editor puts the hold duration on the press item's delay (golden
// vector 1: press 30ms + release 30ms), i.e. a firmware entry waits AFTER its
// action. Legacy timelines carry delays as standalone rows, so each row merges
// into the previous key event; leading rows shift onto the first event and
// trailing rows onto the last one (total duration is preserved either way).
bool LegacyActionsToItems(const json::Value& actions, std::string* out, std::string* error) {
    struct DraftItem {
        std::string key;
        bool press = false;
        long long delay = 0;
    };
    std::vector<DraftItem> items;
    long long pending = 0;
    int count = 0;
    for (const json::Value& act : actions.array) {
        const std::string prefix = "legacy action " + std::to_string(count) + ": ";
        if (act.type != json::Value::Type::kObject) {
            *error = prefix + "must be an object";
            return false;
        }
        std::string type;
        if (!act.GetString("type", &type)) {
            *error = prefix + "missing type";
            return false;
        }
        if (type == "delay") {
            long long ms = 0;
            if (!act.GetInt("ms", &ms) || ms < 0 || ms > 65535) {
                *error = prefix + "ms must be 0..65535";
                return false;
            }
            pending = pending + ms > 65535 ? 65535 : pending + ms;
            ++count;
            continue;
        }
        if (type != "down" && type != "up") {
            *error = prefix + "unknown type: " + type;
            return false;
        }
        std::string key;
        if (!act.GetString("key", &key) || key.empty()) {
            *error = prefix + "missing key";
            return false;
        }
        DraftItem item;
        item.key = LegacyKeyToStudio(key);
        item.press = type == "down";
        if (items.empty()) {
            item.delay = pending;  // no previous item: shift the wait onto this one
        } else {
            DraftItem& last = items.back();
            last.delay = last.delay + pending > 65535 ? 65535 : last.delay + pending;
        }
        pending = 0;
        items.push_back(std::move(item));
        ++count;
    }
    if (!items.empty() && pending > 0) {
        DraftItem& last = items.back();
        last.delay = last.delay + pending > 65535 ? 65535 : last.delay + pending;
    }

    std::string text;
    for (std::size_t i = 0; i < items.size(); ++i) {
        text += "      { \"key\": \"";
        text += json::Escape(items[i].key);
        text += "\", \"press\": ";
        text += items[i].press ? "true" : "false";
        text += ", \"delay\": " + std::to_string(items[i].delay) + " }";
        text += (i + 1 == items.size()) ? "\n" : ",\n";
    }
    *out = text;
    return true;
}

// Rewrites a legacy object as a Studio envelope text and lets the regular
// parser run once over it: one parse authority, byte-identical validation.
bool ConvertLegacyMacroJson(const json::Value& root, std::string* out, std::string* error) {
    std::string name = "导入的宏";
    std::string rawName;
    if (root.GetString("name", &rawName) && !rawName.empty()) name = rawName;
    // The old UI sliced imported names to 32 characters; keep that behaviour.
    const std::wstring wideName = Utf8ToWiden(name);
    std::string trimmed = WidenToUtf8(
        wideName.substr(0, static_cast<std::size_t>(kMacroNameLimit)));

    long long loopCount = 1;
    root.GetInt("loopCount", &loopCount);
    if (loopCount < 1) loopCount = 1;
    if (loopCount > 65535) loopCount = 65535;

    std::string loopMode = "count";
    root.GetString("loopMode", &loopMode);

    const json::Value* actions = root.Find("actions");
    if (actions == nullptr || actions->type != json::Value::Type::kArray) {
        *error = "legacy macro needs an actions array";
        return false;
    }
    std::string items;
    if (!LegacyActionsToItems(*actions, &items, error)) return false;

    *out = "{\n";
    *out += "  \"format\": \"incott-studio-macro\",\n";
    *out += "  \"version\": 1,\n";
    *out += "  \"macro\": {\n";
    *out += "    \"name\": \"" + json::Escape(trimmed) + "\",\n";
    *out += "    \"looptype\": " + std::to_string(LegacyLoopType(loopMode)) + ",\n";
    *out += "    \"cycle\": " + std::to_string(loopCount) + ",\n";
    *out += "    \"uid\": 0,\n";
    *out += "    \"item\": [\n" + items + "\n    ]\n";
    *out += "  }\n";
    *out += "}\n";
    return true;
}

}  // namespace

bool ParseMacroJson(const std::string& text, Macro* macro, std::string* error) {
    if (text.size() > 131072) {
        *error = "file exceeds 128 KB limit";
        return false;
    }
    json::Value root;
    std::string parseError;
    if (!json::Parse(text, &root, &parseError)) {
        *error = "invalid JSON: " + parseError;
        return false;
    }
    // WP1-5: old web-UI exports (incott-g23-macro-v1) are recognized and
    // rewritten as the Studio schema; the Studio parser stays the only authority.
    if (IsLegacyMacroObject(root)) {
        std::string converted;
        if (!ConvertLegacyMacroJson(root, &converted, error)) return false;
        return ParseMacroJson(converted, macro, error);
    }
    const json::Value* body = &root;
    if (root.type == json::Value::Type::kObject) {
        if (root.Find("format") != nullptr || root.Find("version") != nullptr) {
            std::string format;
            if (!root.GetString("format", &format) || format != "incott-studio-macro") {
                *error = "unsupported macro format";
                return false;
            }
            long long version = 0;
            if (!root.GetInt("version", &version) || version != 1) {
                *error = "unsupported format version";
                return false;
            }
            const json::Value* inner = root.Find("macro");
            if (inner == nullptr || inner->type != json::Value::Type::kObject) {
                *error = "macro must be an object";
                return false;
            }
            body = inner;
        } else if (const json::Value* inner = root.Find("macro")) {
            if (inner->type != json::Value::Type::kObject) {
                *error = "macro must be an object";
                return false;
            }
            body = inner;
        }
    }
    return ParseMacroValue(*body, macro, error);
}

bool ParseMacroValue(const json::Value& body, Macro* macro, std::string* error) {
    if (body.type != json::Value::Type::kObject) {
        *error = "macro must be an object";
        return false;
    }

    Macro parsed;
    std::string name;
    if (!body.GetString("name", &name)) {
        *error = "macro needs a name";
        return false;
    }
    parsed.name = Utf8ToWiden(name);
    if (parsed.name.size() > static_cast<std::size_t>(kMacroNameLimit)) {
        *error = "name must be at most 32 characters";
        return false;
    }
    for (wchar_t ch : parsed.name) {
        if (ch < 0x20 || ch == 0x7F) {
            *error = "name contains control characters";
            return false;
        }
    }

    long long loopType = 0;
    if (!body.GetInt("looptype", &loopType) || loopType < 1 || loopType > 3) {
        *error = "looptype must be 1, 2, or 3";
        return false;
    }
    parsed.loopType = static_cast<int>(loopType);

    long long cycles = 1;
    const bool hasCycles = body.GetInt("cycle", &cycles);
    if (hasCycles) {
        if (cycles < 1 || cycles > 65535) {
            *error = "cycles must be 1..65535";
            return false;
        }
        parsed.cycles = static_cast<int>(cycles);
    } else {
        if (parsed.loopType == 3) {
            *error = "cycles must be 1..65535";
            return false;
        }
        parsed.cycles = 1;
    }

    long long uid = 0;
    if (body.GetInt("uid", &uid)) {
        if (uid < 0 || static_cast<unsigned long long>(uid) > 9007199254740991ULL) {
            *error = "uid must be between 0 and 9007199254740991";
            return false;
        }
        parsed.uid = static_cast<std::uint64_t>(uid);
    }

    const json::Value* items = body.Find("item");
    if (items == nullptr || items->type != json::Value::Type::kArray) {
        *error = "macro needs an item array";
        return false;
    }
    if (items->array.empty()) {
        *error = "macro needs at least one item";
        return false;
    }
    if (static_cast<int>(items->array.size()) > kMacroActionLimit) {
        *error = "macro has more than 71 items";
        return false;
    }

    std::set<std::string> held;
    int index = 0;
    for (const json::Value& item : items->array) {
        const std::string prefix = "item " + std::to_string(index) + ": ";
        if (item.type != json::Value::Type::kObject) {
            *error = prefix + "must be an object";
            return false;
        }
        std::string key;
        if (!item.GetString("key", &key)) {
            *error = prefix + "missing key";
            return false;
        }
        int usage = 0;
        if (!KeyUsageFromCode(key.c_str(), &usage)) {
            *error = prefix + "unknown key: " + key;
            return false;
        }
        bool press = false;
        if (!item.GetBool("press", &press)) {
            *error = prefix + "missing press";
            return false;
        }
        long long delay = 0;
        if (!item.GetInt("delay", &delay)) {
            *error = prefix + "missing delay";
            return false;
        }
        if (delay < 0 || delay > 65535) {
            *error = prefix + "delay must be 0..65535";
            return false;
        }

        if (press) {
            if (held.count(key) > 0) {
                *error = prefix + "duplicate press for key: " + key;
                return false;
            }
            held.insert(key);
        } else {
            if (held.count(key) == 0) {
                *error = prefix + "release without press for key: " + key;
                return false;
            }
            held.erase(key);
        }

        MacroAction action;
        action.press = press;
        action.usage = usage;
        action.delayMs = static_cast<int>(delay);
        parsed.actions.push_back(action);
        ++index;
    }

    // docs/05-firmware-constraints.md section 8: the firmware silently drops a
    // dangling press, so a macro that is still holding keys when the import
    // ends gets synthetic zero-delay releases appended here.
    std::string closeError;
    if (!CloseMacroTail(&parsed, &closeError)) {
        *error = closeError;
        return false;
    }
    if (parsed.actions.size() > static_cast<std::size_t>(kMacroActionLimit)) {
        *error = "macro has more than 71 items";
        return false;
    }

    const MacroError code =
        ValidateMacro(0, parsed.loopType, parsed.cycles, parsed.actions);
    if (code != MacroError::kOk) {
        *error = MacroErrorText(code);
        return false;
    }
    *macro = parsed;
    error->clear();
    return true;
}

std::string MacroToJson(const Macro& macro) {
    std::string out;
    out += "{\n";
    out += "  \"format\": \"incott-studio-macro\",\n";
    out += "  \"version\": 1,\n";
    out += "  \"macro\": {\n";
    out += "    \"name\": \"" + json::Escape(WidenToUtf8(macro.name)) + "\",\n";
    out += "    \"looptype\": " + std::to_string(macro.loopType) + ",\n";
    int cycle = macro.cycles;
    if (cycle < 1) cycle = 1;
    if (cycle > 65535) cycle = 65535;
    out += "    \"cycle\": " + std::to_string(cycle) + ",\n";
    out += "    \"uid\": " + std::to_string(macro.uid) + ",\n";
    out += "    \"item\": [\n";
    for (std::size_t i = 0; i < macro.actions.size(); ++i) {
        const MacroAction& action = macro.actions[i];
        out += "      { \"key\": \"";
        out += json::Escape(KeyCodeForUsage(action.usage));
        out += "\", \"press\": ";
        out += action.press ? "true" : "false";
        out += ", \"delay\": " + std::to_string(action.delayMs) + " }";
        out += (i + 1 == macro.actions.size()) ? "\n" : ",\n";
    }
    out += "    ]\n";
    out += "  }\n";
    out += "}\n";
    return out;
}

std::string MacroSummaryJson(const Macro& macro) {
    std::string out = "{\"name\":\"";
    out += json::Escape(WidenToUtf8(macro.name));
    out += "\",\"loop\":" + std::to_string(macro.loopType);
    out += ",\"cycles\":" + std::to_string(macro.cycles);
    out += ",\"actions\":" + std::to_string(macro.actions.size());
    out += "}";
    return out;
}

}  // namespace incott
