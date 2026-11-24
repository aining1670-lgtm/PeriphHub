#pragma once
// Studio macro JSON (docs/05 section 4): import/export, kept separate from the
// local library text form in macro_store.h.
#include <string>

#include "json.h"
#include "macro_store.h"

namespace incott {

// Parses the Studio envelope (or a bare macro object). Errors are one-line and
// name the offending item, e.g. "item 2: unknown key: Space".
bool ParseMacroJson(const std::string& text, Macro* macro, std::string* error);

// Same, from an already parsed macro object (bridge: args.macro).
bool ParseMacroValue(const json::Value& body, Macro* macro, std::string* error);

// Envelope form used for export and for "paste JSON" in the web UI.
std::string MacroToJson(const Macro& macro);

// Compact form for macros.list / macro.import: {"name":..,"loop":..,"cycles":..,"actions":..}
std::string MacroSummaryJson(const Macro& macro);

}  // namespace incott
