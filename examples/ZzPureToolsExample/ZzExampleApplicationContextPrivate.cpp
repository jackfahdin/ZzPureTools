#include "ZzExampleApplicationContextPrivate.h"

#include <QtCore/QDir>

namespace ZzExample {

namespace {

/** @brief 解析当前编译目标的用户可读平台名称。 */
[[nodiscard]] QString zzPlatformName()
{
#if defined(Q_OS_WINDOWS)
    return QStringLiteral("Windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macOS");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("Linux");
#else
    return QStringLiteral("Unknown");
#endif
}

} // namespace

ZzExampleApplicationContextPrivate::ZzExampleApplicationContextPrivate(
    const ZzCore::ZzApplicationPaths &applicationPaths)
    : paths(applicationPaths)
    , settings(QDir(paths.configDirectory()).filePath(
          QStringLiteral("settings.ini")))
    , tasks(0)
    , activities(200)
    , platform(zzPlatformName())
{
}

} // namespace ZzExample
