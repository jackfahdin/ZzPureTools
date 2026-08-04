#include <ZzCore/ZzQtLogBridge.h>

#include "private/ZzQtLogBridgePrivate.h"

namespace ZzCore {

ZzQtLogBridge::ZzQtLogBridge()
    : d_ptr(std::make_unique<ZzQtLogBridgePrivate>())
{
}

ZzQtLogBridge::~ZzQtLogBridge()
{
    if (d_ptr->isInstalled()) {
        static_cast<void>(d_ptr->uninstall());
    }
}

ZzResult<void> ZzQtLogBridge::install(
    const ZzQtLogBridgeConfig &config)
{
    return d_ptr->install(config);
}

ZzResult<void> ZzQtLogBridge::uninstall()
{
    return d_ptr->uninstall();
}

bool ZzQtLogBridge::isInstalled() const noexcept
{
    return d_ptr->isInstalled();
}

} // namespace ZzCore
