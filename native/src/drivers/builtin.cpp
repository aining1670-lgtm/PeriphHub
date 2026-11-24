#include "drivers/registry.h"

#include "drivers/builtin_caps.h"
#include "drivers/host.h"
#include "drivers/royuan_driver.h"
#include "drivers/yjx_driver.h"

namespace drivers {

void RegisterBuiltinDrivers(Registry& r) {
    auto incott = IncottDescriptor();
    incott.handle = host::HandleIncottRequest;
    r.Add(std::move(incott));

    auto yjx = YjxDescriptor();
    yjx.handle = yjx_driver::Handle;
    r.Add(std::move(yjx));

    auto royuan = RoyuanDescriptor();
    royuan.handle = royuan_driver::Handle;
    r.Add(std::move(royuan));

    // Community drivers: add one line here
    r.SetDefault("incott");
}

}  // namespace drivers
