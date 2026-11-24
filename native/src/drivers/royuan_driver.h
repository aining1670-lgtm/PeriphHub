#pragma once
#include <string>

#include "bridge.h"

namespace royuan_driver {

std::string Handle(const bridge::Request& request, const std::string& device_key);

}  // namespace royuan_driver
