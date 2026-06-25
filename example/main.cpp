#include <QApplication>

#include <ZzPureTools/ZzPureTools.hpp>

#include "MainWindow.hpp"

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("ZzPureToolsExample"));
    application.setOrganizationName(QStringLiteral("ZzPureTools"));

    ZzApplication::instance().initialize(application);

    MainWindow window;
    window.show();

    return application.exec();
}
