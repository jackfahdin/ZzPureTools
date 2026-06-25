#include <QApplication>

#include <ZzFluent/ZzFluent.hpp>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    ZzApplication::instance().initialize(application);

    ZzPushButton button(QStringLiteral("Hello ZzFluent"));
    button.setButtonStyle(ZzPushButton::ZzButtonStyle::Accent);
    button.resize(200, 40);
    button.show();

    return application.exec();
}
