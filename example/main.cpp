#include <QApplication>
#include <QCommandLineParser>

#include <ZzPureTools/ZzPureTools.hpp>

#include "MainWindow.hpp"

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("ZzPureToolsExample"));
    application.setOrganizationName(QStringLiteral("ZzPureTools"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("ZzPureTools widget demonstration application"));
    parser.addHelpOption();

    QCommandLineOption themeOption(QStringList() << QStringLiteral("theme") << QStringLiteral("t"),
                                   QStringLiteral("Initial theme mode: light, dark, or system"),
                                   QStringLiteral("mode"), QStringLiteral("system"));
    parser.addOption(themeOption);
    parser.process(application);

    ZzApplication::instance().initialize(application);

    const QString themeValue = parser.value(themeOption).toLower();
    if (ZzTheme* theme = ZzApplication::instance().theme()) {
        if (themeValue == QStringLiteral("light")) {
            theme->setMode(ZzThemeMode::Light);
        }
        else if (themeValue == QStringLiteral("dark")) {
            theme->setMode(ZzThemeMode::Dark);
        }
        // "system" is the default, no explicit change needed.
    }

    MainWindow window;
    window.show();

    return application.exec();
}
