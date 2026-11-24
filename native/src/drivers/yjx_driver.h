#pragma once
#include <string>

#include "bridge.h"

namespace yjx_driver {

std::string Handle(const bridge::Request& request, const std::string& device_key);

}  // namespace yjx_driver
