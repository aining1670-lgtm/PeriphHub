#pragma once
// Host services exposed to extracted drivers (implemented in main.cpp).
#include <string>

#include "bridge.h"

namespace host {

std::string HandleIncottRequest(const bridge::Request& request, const std::string& key);
std::string HandleMacroRequest(const bridge::Request& request,
                               const std::string& driver_name,
                               const std::string& key);
void ClearButtonMacroBind(int button_index);
std::wstring AppDataDir();
bool ReadAllBytes(const std::wstring& path, std::string* out, std::wstring* error,
                  bool* not_found = nullptr);
bool WriteAllBytesAtomic(const std::wstring& path, const std::string& bytes, std::wstring* error);

}  // namespace host
