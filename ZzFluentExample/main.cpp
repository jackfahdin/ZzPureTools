#include <QApplication>

#include "MainWindow.h"
#include "ZzApplication.h"

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("ZzFluentExample"));
    application.setOrganizationName(QStringLiteral("ZzPureTools"));

    ZzApplication::instance().initialize(application);

    MainWindow window;
    window.show();

    return application.exec();
}
