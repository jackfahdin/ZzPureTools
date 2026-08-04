#include <cstdlib>

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include "ZzWindowKitDemoWindow.h"

int main(int argc, char *argv[])
{
    const auto prepared = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!prepared) {
        return EXIT_FAILURE;
    }

    QApplication application(argc, argv);
    qInfo().noquote()
        << "ZzWindowKitDemo platform:"
        << QGuiApplication::platformName();

    ZzWindowKitDemoWindow window;
    window.show();

    bool timeoutValid = false;
    const auto timeout = qEnvironmentVariableIntValue(
        "ZZ_WINDOWKIT_DEMO_AUTO_CLOSE_MS", &timeoutValid);
    if (timeoutValid && timeout > 0) {
        QTimer::singleShot(timeout, &application, &QCoreApplication::quit);
    }
    return application.exec();
}
