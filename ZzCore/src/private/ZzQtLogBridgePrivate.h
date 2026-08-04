#pragma once

#include <QtCore/qlogging.h>

#include <ZzCore/ZzQtLogBridgeConfig.h>
#include <ZzCore/ZzResult.h>

namespace ZzCore {

class ZzQtLogBridgePrivate final
{
public:
    [[nodiscard]] ZzResult<void> install(ZzQtLogBridgeConfig config);
    [[nodiscard]] ZzResult<void> uninstall();
    [[nodiscard]] bool isInstalled() const noexcept;

    static void handleMessage(
        QtMsgType type,
        const QMessageLogContext &context,
        const QString &message);

    ZzQtLogBridgeConfig config;
    bool installed = false;
};

} // namespace ZzCore
