#pragma once
// Typed optional argument extractors for bridge JSON request objects.
// Pure C++17, free of Win32 dependencies.
#include "json.h"

namespace bridge {

enum class ArgState { kAbsent, kOk, kBad };

ArgState OptIntArg(const json::Value* args, const char* name, int* out);
ArgState OptBoolArg(const json::Value* args, const char* name, bool* out);

}  // namespace bridge
