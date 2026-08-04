#include <cstdlib>

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

#include "ZzFluentControlsGallery.h"

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"));
    if (fusion == nullptr) {
        qCritical("ZzFluentControlsGallery requires the Fusion style");
        return EXIT_FAILURE;
    }

    ZzFluentUI::ZzThemeController controller;
    application.setStyle(
        new ZzFluentUI::ZzFluentStyle(&controller, fusion));
    ZzExamples::ZzFluentControlsGallery gallery(&controller);
    gallery.show();

    qInfo().noquote()
        << "ZzFluentControlsGallery platform:"
        << QGuiApplication::platformName();
    bool timeoutValid = false;
    const int timeout = qEnvironmentVariableIntValue(
        "ZZ_FLUENT_GALLERY_AUTO_CLOSE_MS",
        &timeoutValid);
    if (timeoutValid && timeout > 0) {
        QTimer::singleShot(
            timeout,
            &application,
            &QCoreApplication::quit);
    }

    const int exitCode = application.exec();
    application.setStyle(
        QStyleFactory::create(QStringLiteral("Fusion")));
    return exitCode == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
