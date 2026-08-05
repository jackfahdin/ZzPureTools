#include <ZzCore/ZzCoreVersion.h>
#include <ZzFluentUI/ZzFluentVersion.h>
#include <ZzFluentUI/ZzFluentWidgetVersion.h>
#include <ZzPureTools/ZzAppCoreVersion.h>
#include <ZzPureTools/ZzPureToolsVersion.h>
#include <ZzWindowKit/ZzWindowKitVersion.h>

#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QSysInfo>
#include <QtGui/QGuiApplication>

#if defined(Q_OS_WIN)
#  include <windows.h>
static_assert(sizeof(void *) == 8);
#elif defined(Q_OS_MACOS)
#  include <TargetConditionals.h>
#  if !defined(__arm64__) && !defined(__x86_64__)
#    error Unsupported macOS architecture
#  endif
#elif defined(Q_OS_LINUX)
#  include <unistd.h>
#else
#  error Unsupported platform
#endif

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    const QStringList versions{
        ZzCore::ZzCoreVersion::toString(),
        ZzWindowKit::ZzWindowKitVersion::toString(),
        ZzFluentUI::ZzFluentVersion::toString(),
        ZzFluentUI::ZzFluentWidgetVersion::toString(),
        ZzPureTools::ZzAppCoreVersion::toString(),
        ZzPureTools::ZzPureToolsVersion::toString(),
    };
    for (const QString &version : versions) {
        if (version.isEmpty()) {
            return 1;
        }
    }
#if defined(Q_OS_WIN)
    if (GetModuleHandleW(nullptr) == nullptr) {
        return 2;
    }
#elif defined(Q_OS_MACOS)
    if (TARGET_OS_OSX == 0) {
        return 3;
    }
#elif defined(Q_OS_LINUX)
    if (getpid() <= 0) {
        return 4;
    }
#endif
    qInfo().noquote() << QSysInfo::prettyProductName()
                      << QSysInfo::buildCpuArchitecture()
                      << QGuiApplication::platformName();
    return 0;
}
