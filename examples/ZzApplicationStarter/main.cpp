#include <cstdlib>
#include <memory>

#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryDir>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include "ZzStarterSetup.h"

int main(int argc, char *argv[])
{
    if (!ZzWindowKit::ZzWindowKitBootstrap::prepare()) {
        return EXIT_FAILURE;
    }
    ZzPureTools::ZzPureApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("ZzStarter"));
    application.setApplicationName(QStringLiteral("ZzApplicationStarter"));
    const bool smoke = application.arguments().contains(QStringLiteral("--smoke"));
    QTemporaryDir temporarySettings;
    if (smoke && !temporarySettings.isValid()) {
        return EXIT_FAILURE;
    }
    auto settings = smoke
        ? std::make_unique<QSettings>(temporarySettings.filePath(QStringLiteral("settings.ini")),
            QSettings::IniFormat)
        : std::make_unique<QSettings>();
    if (smoke) {
        // A non-default stored value proves startup restores the preference.
        settings->setValue(QStringLiteral("themeMode"), 3);
    }
    const auto built = zzBuildStarter(application, *settings);
    if (!built) {
        qCritical().noquote() << built.error().technicalMessage();
        application.beginShutdown();
        return EXIT_FAILURE;
    }
    const int result = smoke ? zzRunStarterSmoke(application, *settings) : application.exec();
    // Settings captured by callbacks must outlive all application windows.
    application.beginShutdown();
    return result;
}
