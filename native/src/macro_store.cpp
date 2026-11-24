#include "macro_store.h"

#include <sstream>

namespace incott {
namespace {

constexpr wchar_t kHeader[] = L"periphhub-macros 1";

std::wstring Widen(const char* text) {
    std::wstring out;
    for (const char* p = text; *p != '\0'; ++p) out.push_back(static_cast<wchar_t>(*p));
    return out;
}

}  // namespace

bool CloseMacroTail(Macro* macro, std::string* error) {
    if (macro->actions.empty()) {
        *error = "macro needs at least one item";
        return false;
    }
    std::vector<int> held_stack;
    for (const MacroAction& action : macro->actions) {
        if (action.press) {
            held_stack.push_back(action.usage);
        } else {
            for (auto it = held_stack.rbegin(); it != held_stack.rend(); ++it) {
                if (*it == action.usage) {
                    held_stack.erase(std::next(it).base());
                    break;
                }
            }
        }
    }
    if (held_stack.empty()) return true;
    if (macro->actions.size() + held_stack.size() > static_cast<std::size_t>(incott::kMacroActionLimit)) {
        *error = "last action is a press; the 71-action limit leaves no room for its release";
        return false;
    }
    for (auto it = held_stack.rbegin(); it != held_stack.rend(); ++it) {
        MacroAction release;
        release.press = false;
        release.usage = *it;
        release.delayMs = 0;
        macro->actions.push_back(release);
    }
    return true;
}

MacroSlot& MacroStore::At(int slot) { return slots_[slot]; }
const MacroSlot& MacroStore::At(int slot) const { return slots_[slot]; }

int MacroStore::Binding(int keyIndex) const {
    if (keyIndex < 0 || keyIndex >= kKeyCount) return -1;
    return bindings_[keyIndex];
}

void MacroStore::SetBinding(int keyIndex, int slot) {
    if (keyIndex < 0 || keyIndex >= kKeyCount) return;
    bindings_[keyIndex] = (slot >= 0 && slot < kSlotCount) ? slot : -1;
}

std::wstring MacroStore::Serialize() const {
    std::wstring out = kHeader;
    out += L"\n";
    for (int i = 0; i < kSlotCount; ++i) {
        const MacroSlot& slot = slots_[i];
        if (!slot.defined) continue;
        out += L"slot " + std::to_wstring(i) + L"\n";
        out += L"name " + slot.macro.name + L"\n";
        out += L"loop " + std::to_wstring(slot.macro.loopType) + L"\n";
        out += L"cycles " + std::to_wstring(slot.macro.cycles) + L"\n";
        out += L"uid " + std::to_wstring(slot.macro.uid) + L"\n";
        for (const MacroAction& action : slot.macro.actions) {
            out += L"action " + std::to_wstring(action.press ? 1 : 0) + L" " +
                   std::to_wstring(action.usage) + L" " + std::to_wstring(action.delayMs) + L"\n";
        }
    }
    for (int key = 0; key < kKeyCount; ++key) {
        if (bindings_[key] < 0) continue;
        out += L"bind " + std::to_wstring(key) + L" " + std::to_wstring(bindings_[key]) + L"\n";
    }
    return out;
}

bool MacroStore::Deserialize(const std::wstring& text, std::wstring* error) {
    // Parse into locals and commit only on success: a rejected file must not
    // clobber the macros the user already has. Line numbers below are physical,
    // 1-based lines of the file (the header is line 1).
    std::array<MacroSlot, kSlotCount> slots{};
    std::array<int, kKeyCount> bindings;
    bindings.fill(-1);

    std::wistringstream stream(text);
    std::wstring line;
    int lineNo = 0;
    if (!std::getline(stream, line)) {
        *error = L"line 1: missing file header";
        return false;
    }
    ++lineNo;
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    if (line != kHeader) {
        *error = L"line 1: missing file header";
        return false;
    }

    int current = -1;
    while (std::getline(stream, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty()) continue;

        std::wistringstream parts(line);
        std::wstring keyword;
        if (!(parts >> keyword)) {
            *error = L"line " + std::to_wstring(lineNo) + L": unknown keyword";
            return false;
        }

        if (keyword == L"slot") {
            long long index = -1;
            std::wstring extra;
            if (!(parts >> index) || (parts >> extra) || index < 0 || index >= kSlotCount) {
                *error = L"line " + std::to_wstring(lineNo) + L": slot must be 0..9";
                return false;
            }
            current = static_cast<int>(index);
            slots[current].defined = true;
            slots[current].dirty = true;
            continue;
        }
        if (keyword == L"bind") {
            long long key = -1;
            long long slot = -1;
            std::wstring extra;
            if (!(parts >> key) || !(parts >> slot) || (parts >> extra) || key < 0 ||
                key >= kKeyCount || slot < 0 || slot >= kSlotCount) {
                *error = L"line " + std::to_wstring(lineNo) + L": bind needs key 0..5 and slot 0..9";
                return false;
            }
            bindings[static_cast<int>(key)] = static_cast<int>(slot);
            continue;
        }
        if (current < 0) {
            *error = L"line " + std::to_wstring(lineNo) + L": entry before any slot";
            return false;
        }

        Macro& macro = slots[current].macro;
        if (keyword == L"name") {
            std::wstring rest;
            std::getline(parts, rest);
            if (!rest.empty() && rest.front() == L' ') rest.erase(0, 1);
            for (wchar_t ch : rest) {
                if (ch < 0x20 || ch == 0x7F) {
                    *error = L"line " + std::to_wstring(lineNo) + L": name contains control characters";
                    return false;
                }
            }
            macro.name = rest;
        } else if (keyword == L"loop") {
            long long loopType = 0;
            std::wstring extra;
            if (!(parts >> loopType) || (parts >> extra) || loopType < 1 || loopType > 3) {
                *error = L"line " + std::to_wstring(lineNo) + L": loop type must be 1..3";
                return false;
            }
            macro.loopType = static_cast<int>(loopType);
        } else if (keyword == L"cycles") {
            long long cycles = 0;
            std::wstring extra;
            if (!(parts >> cycles) || (parts >> extra) || cycles < 1 || cycles > 65535) {
                *error = L"line " + std::to_wstring(lineNo) + L": cycles must be 1..65535";
                return false;
            }
            macro.cycles = static_cast<int>(cycles);
        } else if (keyword == L"uid") {
            unsigned long long uid = 0;
            std::wstring extra;
            if (!(parts >> uid) || (parts >> extra) || uid > 9007199254740991ULL) {
                *error = L"line " + std::to_wstring(lineNo) + L": uid must be 0..9007199254740991";
                return false;
            }
            macro.uid = static_cast<std::uint64_t>(uid);
        } else if (keyword == L"action") {
            long long press = 0;
            long long usage = 0;
            long long delay = 0;
            std::wstring extra;
            if (!(parts >> press) || !(parts >> usage) || !(parts >> delay) ||
                (parts >> extra) || (press != 0 && press != 1) || usage < 0 || usage > 0xFF ||
                delay < 0 || delay > 0xFFFF) {
                *error = L"line " + std::to_wstring(lineNo) +
                         L": action needs press 0|1, usage 0..255, delay 0..65535";
                return false;
            }
            MacroAction action;
            action.press = press != 0;
            action.usage = static_cast<int>(usage);
            action.delayMs = static_cast<int>(delay);
            macro.actions.push_back(action);
        } else {
            *error = L"line " + std::to_wstring(lineNo) + L": unknown keyword";
            return false;
        }
    }

    for (int i = 0; i < kSlotCount; ++i) {
        MacroSlot& slot = slots[i];
        if (!slot.defined) continue;
        if (slot.macro.name.size() > static_cast<std::size_t>(kMacroNameLimit)) {
            *error = L"slot " + std::to_wstring(i) + L": name is longer than 32 characters";
            return false;
        }
        for (wchar_t ch : slot.macro.name) {
            if (ch < 0x20 || ch == 0x7F) {
                *error = L"slot " + std::to_wstring(i) + L": name contains control characters";
                return false;
            }
        }
        const MacroError code = ValidateMacro(0, slot.macro.loopType, slot.macro.cycles,
                                              slot.macro.actions);
        if (code != MacroError::kOk) {
            *error = L"slot " + std::to_wstring(i) + L": " + Widen(MacroErrorText(code));
            return false;
        }
    }
    slots_ = slots;
    bindings_ = bindings;
    *error = L"";
    return true;
}

}  // namespace incott
