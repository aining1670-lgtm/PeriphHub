#include "bridge_args.h"

#include <climits>

namespace bridge {

ArgState OptIntArg(const json::Value* args, const char* name, int* out) {
    if (args == nullptr) return ArgState::kAbsent;
    const json::Value* v = args->Find(name);
    if (v == nullptr) return ArgState::kAbsent;
    if (v->type != json::Value::Type::kNumber || v->number < INT_MIN || v->number > INT_MAX) {
        return ArgState::kBad;
    }
    *out = static_cast<int>(v->number);
    return ArgState::kOk;
}

ArgState OptBoolArg(const json::Value* args, const char* name, bool* out) {
    if (args == nullptr) return ArgState::kAbsent;
    const json::Value* v = args->Find(name);
    if (v == nullptr) return ArgState::kAbsent;
    if (v->type != json::Value::Type::kBool) return ArgState::kBad;
    *out = v->boolean;
    return ArgState::kOk;
}

}  // namespace bridge
