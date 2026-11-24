#pragma once
// Local macro library: the firmware cannot read macros back, so the PC owns them.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "protocol.h"

namespace incott {

struct Macro {
    std::wstring name;
    int loopType = 3;
    int cycles = 1;
    std::uint64_t uid = 0;
    std::vector<MacroAction> actions;
};

struct MacroSlot {
    Macro macro;
    bool defined = false;
    bool dirty = true;   // dirty means "must be uploaded before the next apply"
};

// A macro must not end with a dangling press: the firmware drops a trailing
// press that has no release (measured on G23: a 3-item A↓/A↑/B↓ macro types
// only "a"). Saving appends the release so the last item is always a key-up.
bool CloseMacroTail(Macro* macro, std::string* error);

class MacroStore {
public:
    static constexpr int kSlotCount = 10;

    MacroSlot& At(int slot);
    const MacroSlot& At(int slot) const;

    // Key index (0..5) -> library slot, or -1 when the key does not play a macro.
    int Binding(int keyIndex) const;
    void SetBinding(int keyIndex, int slot);

    // UTF-16 text form. The caller owns the file
    // (%LOCALAPPDATA%\PeriphHub\macros.ini), so this stays Win32-free and testable.
    std::wstring Serialize() const;
    bool Deserialize(const std::wstring& text, std::wstring* error);

private:
    std::array<MacroSlot, kSlotCount> slots_{};
    std::array<int, kKeyCount> bindings_{{ -1, -1, -1, -1, -1, -1 }};
};

}  // namespace incott
