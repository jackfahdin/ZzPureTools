#include <cstdlib>

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ZzFluentUI::ZzThemeController controller;
    application.setStyle(new ZzFluentUI::ZzFluentStyle(&controller));

    QWidget window;
    window.setWindowTitle(QStringLiteral("ZzFluentUI"));
    auto *layout = new QVBoxLayout(&window);
    auto *platform = new QLabel(
        QGuiApplication::platformName(),
        &window);
    auto *mode = new QComboBox(&window);
    mode->addItems({
        QStringLiteral("System"),
        QStringLiteral("Light"),
        QStringLiteral("Dark"),
        QStringLiteral("HighContrast")});
    layout->addWidget(platform);
    layout->addWidget(mode);
    QObject::connect(
        mode,
        &QComboBox::currentIndexChanged,
        &controller,
        [&controller](int index) {
            controller.setMode(
                static_cast<ZzFluentUI::ZzThemeMode>(index));
        });

    qInfo().noquote()
        << "ZzFluentFoundationDemo platform:"
        << QGuiApplication::platformName();
    bool timeoutValid = false;
    const int timeout = qEnvironmentVariableIntValue(
        "ZZ_FLUENT_DEMO_AUTO_CLOSE_MS",
        &timeoutValid);
    if (timeoutValid && timeout > 0) {
        QTimer::singleShot(
            timeout,
            &application,
            &QCoreApplication::quit);
    }

    window.resize(420, 180);
    window.show();
    const int exitCode = application.exec();
    application.setStyle(
        QStyleFactory::create(QStringLiteral("Fusion")));
    return exitCode == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
