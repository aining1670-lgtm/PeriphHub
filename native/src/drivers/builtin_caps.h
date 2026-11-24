#pragma once
// Built-in driver descriptors and capability JSON builders.
// Kept free of Win32 dependencies so they can be unit-tested standalone.
#include <string>

#include "drivers/registry.h"

namespace drivers {

std::string BuildEmptyCapabilities();

DriverDescriptor IncottDescriptor();
DriverDescriptor YjxDescriptor();
DriverDescriptor RoyuanDescriptor();

}  // namespace drivers
